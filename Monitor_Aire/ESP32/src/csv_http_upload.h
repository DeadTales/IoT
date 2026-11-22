#pragma once

#include <Arduino.h>
#include <FS.h>

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>

// El ciclo prepara un lote en RAM y la tarea de red solo lo envía.
class CsvHttpUpload {
  public:
    void configure(const char *url, const char *token, const char *deviceId);
    bool enabled() const;
    bool ready() const;

    // Solo se llaman desde loop(). Un desplazamiento cero indica que no se envió nada.
    size_t submit(File &file, const char *csvHeader);
    bool takeConfirmation();

  private:
    enum class State { Idle, Uploading, Confirmed, Failed };

    static void runTask(void *context);
    void run();
    bool upload();

    bool configured_ = false;
    TaskHandle_t worker_ = nullptr;
    std::atomic<State> state_{State::Idle};
    std::unique_ptr<uint8_t[]> body_;
    size_t bodySize_ = 0;
    size_t recordCount_ = 0;
    String endpoint_;
    String token_;
};
