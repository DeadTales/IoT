#include "csv_http_upload.h"

#include <HTTPClient.h>
#include <WiFiClient.h>

#include <cstring>
#include <new>

namespace {
constexpr size_t MAX_BATCH_BYTES = 8192;
constexpr size_t MAX_BATCH_RECORDS = 50;
constexpr uint32_t WORKER_STACK_BYTES = 6144;
constexpr int32_t CONNECT_TIMEOUT_MS = 3000;
constexpr uint16_t RESPONSE_TIMEOUT_MS = 5000;
constexpr char HTTP_SCHEME[] = "http://";
constexpr char TOKEN_HEADER[] = "X-CSV-Token";
constexpr char RECORD_COUNT_HEADER[] = "X-CSV-Records";
} // namespace

void CsvHttpUpload::configure(const char *url, const char *token, const char *deviceId) {
    if (*url == '\0' || *token == '\0') {
        if (*url != '\0' || *token != '\0') {
            Serial.println("CSV HTTP: configura URL y token para activar el reenvio");
        }
        return;
    }
    configured_ = true;

    String baseUrl(url);
    baseUrl.trim();
    while (baseUrl.endsWith("/")) {
        baseUrl.remove(baseUrl.length() - 1);
    }
    const size_t hostStart = sizeof(HTTP_SCHEME) - 1;
    if (!baseUrl.startsWith(HTTP_SCHEME) || baseUrl.length() <= hostStart ||
        baseUrl.indexOf('/', hostStart) >= 0 || baseUrl.indexOf('@', hostStart) >= 0 ||
        baseUrl.indexOf('?') >= 0 || baseUrl.indexOf('#') >= 0 || std::strpbrk(token, "\r\n")) {
        Serial.println("CSV HTTP: URL base o token invalido; archivo conservado");
        return;
    }

    endpoint_ = baseUrl + "/api/ingest/" + deviceId + "/csv";
    token_ = token;
    body_.reset(new (std::nothrow) uint8_t[MAX_BATCH_BYTES]);
    if (endpoint_.isEmpty() || token_.isEmpty() || !body_) {
        body_.reset();
        Serial.println("CSV HTTP: memoria insuficiente; archivo conservado");
        return;
    }

    if (xTaskCreate(runTask, "csv-http", WORKER_STACK_BYTES, this, tskIDLE_PRIORITY + 1,
                    &worker_) != pdPASS) {
        worker_ = nullptr;
        body_.reset();
        Serial.println("CSV HTTP: no se pudo iniciar la tarea; archivo conservado");
        return;
    }

    Serial.println("CSV HTTP: reenvio habilitado");
}

bool CsvHttpUpload::enabled() const {
    return configured_;
}

bool CsvHttpUpload::ready() const {
    return worker_ != nullptr && state_.load(std::memory_order_acquire) == State::Idle;
}

size_t CsvHttpUpload::submit(File &file, const char *csvHeader) {
    if (!ready()) {
        return 0;
    }

    const size_t startOffset = file.position();
    const size_t headerSize = std::strlen(csvHeader);
    if (startOffset == static_cast<size_t>(-1) || headerSize >= MAX_BATCH_BYTES) {
        return 0;
    }

    const size_t available = MAX_BATCH_BYTES - headerSize;
    std::memcpy(body_.get(), csvHeader, headerSize);
    const size_t readSize = file.read(body_.get() + headerSize, available);
    if (readSize > available) {
        return 0;
    }

    size_t completeBytes = 0;
    size_t records = 0;

    // Los bytes posteriores al último salto de línea se quedan en LittleFS.
    for (size_t i = 0; i < readSize && records < MAX_BATCH_RECORDS; ++i) {
        if (body_[headerSize + i] == '\n') {
            completeBytes = i + 1;
            ++records;
        }
    }
    if (records == 0) {
        return 0;
    }

    bodySize_ = headerSize + completeBytes;
    recordCount_ = records;

    // El estado transfiere el búfer a la tarea de red hasta que esta publique el resultado.
    state_.store(State::Uploading, std::memory_order_release);
    xTaskNotifyGive(worker_);
    return startOffset + completeBytes;
}

bool CsvHttpUpload::takeConfirmation() {
    const State state = state_.load(std::memory_order_acquire);
    if (state == State::Confirmed || state == State::Failed) {
        state_.store(State::Idle, std::memory_order_release);
    }
    return state == State::Confirmed;
}

void CsvHttpUpload::runTask(void *context) {
    static_cast<CsvHttpUpload *>(context)->run();
}

void CsvHttpUpload::run() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (state_.load(std::memory_order_acquire) == State::Uploading) {
            state_.store(upload() ? State::Confirmed : State::Failed, std::memory_order_release);
        }
    }
}

bool CsvHttpUpload::upload() {
    WiFiClient transport;
    HTTPClient request;
    if (!request.begin(transport, endpoint_)) {
        Serial.println("CSV HTTP: no se pudo preparar la solicitud");
        return false;
    }

    request.setConnectTimeout(CONNECT_TIMEOUT_MS);
    request.setTimeout(RESPONSE_TIMEOUT_MS);
    request.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    request.addHeader("Content-Type", "text/csv");
    request.addHeader(TOKEN_HEADER, token_);

    const char *headers[] = {RECORD_COUNT_HEADER};
    request.collectHeaders(headers, 1);
    const int status = request.POST(body_.get(), bodySize_);
    const bool confirmed = status == HTTP_CODE_NO_CONTENT &&
                           request.header(RECORD_COUNT_HEADER) == String(recordCount_);
    request.end();

    if (!confirmed) {
        Serial.printf("CSV HTTP: respuesta sin confirmacion (%d); archivo conservado\n", status);
    }
    return confirmed;
}
