<script>
  import { Chart, Filler, LineController, LineElement, LinearScale, PointElement, Tooltip } from 'chart.js';
  import { formatDate, formatValue } from '../../models/telemetry';

  Chart.register(Filler, LineController, LineElement, LinearScale, PointElement, Tooltip);
  const axisFont = { size: 12, family: 'IBM Plex Mono, ui-monospace, monospace' };
  const axisNumber = new Intl.NumberFormat('es-MX', { maximumFractionDigits: 2 });
  const gridColor = '#314039';
  const tickColor = '#a9b9ae';
  const cursor = {
    id: 'measurementCursor',
    afterDatasetsDraw(chart) {
      const [point] = chart.getActiveElements();
      if (!point) return;

      const { ctx, chartArea } = chart;
      const { x } = point.element;
      ctx.save();
      ctx.strokeStyle = '#a9b9ae80';
      ctx.setLineDash([3, 4]);
      ctx.beginPath();
      ctx.moveTo(x, chartArea.top);
      ctx.lineTo(x, chartArea.bottom);
      ctx.stroke();
      ctx.restore();
    },
  };

  let { series, period, metric } = $props();
  let canvas = $state();

  $effect(() => {
    if (!canvas) return;
    const selection = metric;
    const range = period;
    const start = Date.parse(series.start);
    const end = Date.parse(series.end);
    const step = series.step_seconds * 1000;
    const values = new Map(series.points.map(point => [Date.parse(point.bucket), point[selection.key]]));
    const points = Array.from({ length: Math.floor((end - start) / step) + 1 }, (_, index) => {
      const time = start + index * step;
      return { x: time, y: values.get(time) ?? null };
    });

    const chart = new Chart(canvas, {
      type: 'line',
      plugins: [cursor],
      data: {
        datasets: [{
          label: selection.label,
          data: points,
          borderWidth: 2,
          borderColor: selection.color,
          backgroundColor: `${selection.color}14`,
          fill: true,
          pointBackgroundColor: selection.color,
          pointBorderWidth: 0,
          pointRadius: 2,
          pointHoverRadius: 5,
          spanGaps: false,
        }],
      },
      options: {
        parsing: false,
        normalized: true,
        maintainAspectRatio: false,
        animation: false,
        interaction: { intersect: false, mode: 'nearest' },
        layout: { padding: { top: 8, right: 12 } },
        scales: {
          x: {
            type: 'linear', min: start, max: end,
            border: { color: gridColor },
            grid: { color: gridColor, drawTicks: false },
            afterBuildTicks: scale => {
              const divisions = scale.chart.width < 480 ? 3 : 6;
              scale.ticks = Array.from({ length: divisions + 1 }, (_, index) => ({
                value: start + index * (end - start) / divisions,
              }));
            },
            ticks: {
              color: tickColor, maxRotation: 0, padding: 11,
              font: axisFont,
              callback: value => formatDate(value, range === 'day' ? 'hour' : 'day'),
            },
          },
          y: {
            grace: '18%',
            border: { color: gridColor },
            grid: { color: gridColor, drawTicks: false },
            ticks: {
              color: tickColor, padding: 11, maxTicksLimit: 6,
              font: axisFont,
              callback: value => `${axisNumber.format(value)} ${selection.unit}`,
            },
          },
        },
        plugins: {
          tooltip: {
            displayColors: false,
            backgroundColor: '#25332c',
            borderColor: '#607566',
            borderWidth: 1,
            cornerRadius: 2,
            padding: 11,
            bodyFont: axisFont,
            callbacks: {
              title: ([entry]) => `${formatDate(entry.parsed.x, 'day')} · ${formatDate(entry.parsed.x, 'hour')}`,
              label: entry => `${formatValue(entry.parsed.y, selection)} ${selection.unit}`,
            },
          },
        },
      },
    });
    return () => chart.destroy();
  });
</script>

<div class="chart-board" role="img" aria-label={`Gráfica de ${metric.label.toLowerCase()} en ${metric.unit}`}>
  <canvas bind:this={canvas} aria-hidden="true"></canvas>
</div>
