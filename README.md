# CalcA

A calculator for the Linux desktop that works like the Windows calculator: simple and
clear in **Standard** mode, switchable to **Scientific**, with **DEC / HEX / BIN** in
both modes. Built with Qt 6 / QML in a dark cyberpunk look; all math is done by
[libqalculate](https://github.com/Qalculate/libqalculate) (exact fractions, 40 internal digits).

## Features

- **Standard:** `%`, `CE`, `C`, `⌫`, `1/x`, `x²`, `√x`, `+ − × ÷`, `±`, decimal separator, `=`
  (pressing `=` again repeats the last operation)
- **Scientific:** `2nd`, `π`, `e`, parentheses, `xʸ`, `ʸ√x`, `10ˣ`, `2ˣ`, `log`, `logᵧx`,
  `ln`, `eˣ`, `n!`, `mod`, `exp`, `|x|`, DEG / RAD / GRAD, F-E, drop-downs for
  trigonometry (incl. inverse and hyperbolic) and functions (floor, ceil, rand, →dms, →deg)
- **DEC / HEX / BIN** switch in the header; HEX adds keys A–F, BIN allows only 0 and 1,
  integers up to 64 bit and beyond, grouped by four digits
- Operator precedence in both modes (`2 + 3 × 4 = 14`)
- Memory (`MC MR M+ M− MS`) and history; from 640 px window width they stay visible on the right
- Full keyboard control (digits, `+ - * / ^ ( ) % !`, Enter, Backspace, Delete = CE,
  Esc = C, a–f in HEX), `Ctrl+C` / `Ctrl+V` copy and paste the number
- Follows the system language (English, German) and number format
- Remembers mode, number system, angle unit and window size

## Build

Dependencies (Fedora):

```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel libqalculate-devel
```

Debian/Ubuntu: `cmake g++ qt6-base-dev qt6-declarative-dev qt6-tools-dev libqalculate-dev`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Afterwards CalcA is in the application menu; typing `calc` finds it.
(KDE: run `kbuildsycoca6` if the entry does not show up right away.)

## Translations

Texts are in `i18n/`. To add a language, add its code to `I18N_TRANSLATED_LANGUAGES`
in `CMakeLists.txt`, run `cmake --build build --target update_translations` and translate
the new `.ts` file (e.g. with Qt Linguist).

## License

GPL-2.0-or-later (see `LICENSE`). Fonts Orbitron, Rajdhani and Share Tech Mono are
under the SIL Open Font License (see `fonts/OFL-*.txt`).

---

## Deutsch

CalcA ist ein Taschenrechner wie unter Windows: einfach im Modus **Standard**,
umschaltbar auf **Wissenschaftlich**, mit **DEC / HEX / BIN** in beiden Modi.
Die Sprache und das Zahlenformat richten sich nach dem System.
Bauen und Installieren wie oben beschrieben; danach findet das Startmenü ihn unter `calc`.
