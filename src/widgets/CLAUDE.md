# src/widgets: reusable Qt widgets

- `collapsiblesection.*`: section with optional header switch (`addSwitch`/`setSwitchedOn`); used by the Simulation and Appearance docks.
- `temperatureslider.*`: vertical temperature-coloured slider, stays active during a run.
- `simulationchart.*`: live charts (CuteChart `ListChart`), rolling point limit, throttled axis formatting.
- `elementpicker.*`: element strip plus periodic-table popup, visible in Build mode.
- `commandpalette.*`: Ctrl+K palette; collects every menu action by walking `menuBar()`.
- `breadcrumbbar.*`, `colorswatch.h`: path bar and shared colour-swatch helpers.
- Widgets carry no viewer logic; they emit signals that `MainWindow` wires. Architecture notes: `docs/architecture/viewer-ui.md`.
