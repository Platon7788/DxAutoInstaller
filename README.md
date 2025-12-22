# DxAutoInstaller C++ Edition

**Version 1.0.0**

---

## 🇬🇧 English

DevExpress VCL Components Automatic Installer — completely rewritten in C++Builder.

### ✨ Features

- **No JCL dependency** — direct Windows Registry access
- **No DevExpress UI dependency** — standard VCL components only
- **RAD Studio 12/13 support** — BDS 23.0 (Athens) and BDS 37.0 (Florence)
- **64-bit IDE support** — design-time packages for both 32-bit and 64-bit IDE
- **Both IDE mode** — install for both IDE versions in single pass
- **Win64 Modern (Clang/LLVM)** — automatic .hpp and .a generation for bcc64x
- **DevExpress VCL 25.1.x** — full support
- **Clean uninstall** — complete removal of all compiled files

### 🚀 IDE Type Options

| Option | Description |
|--------|-------------|
| 32-bit IDE | Design-time packages compiled with dcc32, registered to `Known Packages` |
| 64-bit IDE | Design-time packages compiled with dcc64, registered to `Known Packages x64` |
| Both (32 and 64-bit) | Compile and register for both IDE versions in single installation |

### 📋 Compilation Strategy

**32-bit IDE Mode:**
- Design-time: dcc32 → `Bpl\` → `Known Packages`
- Runtime: Win32, Win64, Win64x (if enabled)

**64-bit IDE Mode:**
- Design-time: dcc64 → `Bpl\Win64\` → `Known Packages x64`
- Runtime: Win64, Win64x only (Win32 disabled)

**Both IDE Mode:**
- Design-time 32-bit: dcc32 → `Bpl\` → `Known Packages`
- Design-time 64-bit: dcc64 → `Bpl\Win64\` → `Known Packages x64`
- Runtime: Win32 and Win64/Win64x

### 🔧 Win64x (Modern C++) Support

When "Install to C++Builder" is enabled with Win64 compilation:
1. `.hpp` files copied from Win64 to Win64x folder
2. `.bpi` files copied to Win64x DCP folder
3. `.a` import libraries generated via `mkexp.exe`

### 🛠️ Building

1. Open `DxAutoInstaller.cbproj` in RAD Studio 12+
2. Select Win64 Modern (Clang) platform
3. Build

**Tested:** Successfully built and tested with RAD Studio 13 Florence, Win64 Modern (x64 Clang) platform.

### 🐛 Bug Reports

**Please report any issues you find!**

This will help improve the program in future versions. Create an Issue on GitHub or email me.

---

## 🇷🇺 Русский

Автоматический установщик компонентов DevExpress VCL — полностью переписан на C++Builder.

### ✨ Возможности

- **Без зависимости от JCL** — прямой доступ к Windows Registry
- **Без зависимости от DevExpress UI** — только стандартные VCL компоненты
- **Поддержка RAD Studio 12/13** — BDS 23.0 (Athens) и BDS 37.0 (Florence)
- **Поддержка 64-bit IDE** — design-time пакеты для 32-bit и 64-bit IDE
- **Режим Both IDE** — установка для обеих версий IDE за один проход
- **Win64 Modern (Clang/LLVM)** — автоматическая генерация .hpp и .a для bcc64x
- **DevExpress VCL 25.1.x** — полная поддержка
- **Чистое удаление** — полное удаление всех скомпилированных файлов

### 🚀 Опции типа IDE

| Опция | Описание |
|-------|----------|
| 32-bit IDE | Design-time пакеты компилируются dcc32, регистрируются в `Known Packages` |
| 64-bit IDE | Design-time пакеты компилируются dcc64, регистрируются в `Known Packages x64` |
| Both (32 and 64-bit) | Компиляция и регистрация для обеих версий IDE за одну установку |

### 📋 Стратегия компиляции

**Режим 32-bit IDE:**
- Design-time: dcc32 → `Bpl\` → `Known Packages`
- Runtime: Win32, Win64, Win64x (если включено)

**Режим 64-bit IDE:**
- Design-time: dcc64 → `Bpl\Win64\` → `Known Packages x64`
- Runtime: только Win64, Win64x (Win32 отключен)

**Режим Both IDE:**
- Design-time 32-bit: dcc32 → `Bpl\` → `Known Packages`
- Design-time 64-bit: dcc64 → `Bpl\Win64\` → `Known Packages x64`
- Runtime: Win32 и Win64/Win64x

### 🔧 Поддержка Win64x (Modern C++)

При включении "Install to C++Builder" с компиляцией Win64:
1. `.hpp` файлы копируются из Win64 в папку Win64x
2. `.bpi` файлы копируются в папку Win64x DCP
3. `.a` import библиотеки генерируются через `mkexp.exe`

### 🛠️ Сборка

1. Откройте `DxAutoInstaller.cbproj` в RAD Studio 12+
2. Выберите платформу Win64 Modern (Clang)
3. Build

**Проверено:** Успешно собрано и протестировано в RAD Studio 13 Florence, платформа Win64 Modern (x64 Clang).

### 🐛 Сообщения об ошибках

**Пожалуйста, сообщайте о найденных проблемах!**

Это поможет улучшить программу в будущих версиях. Создайте Issue на GitHub или напишите на email.

---

## 📁 Directory Structure / Структура директорий

```
Library\
├── Sources\              # ALL sources / ВСЕ исходники (.pas, .dfm, .res)
│
├── 290\                  # RAD Studio 12 compiled files
│   ├── *.dcu             # Win32 compiled units
│   ├── Win64\*.dcu       # Win64 compiled units
│   └── Win64x\
│       ├── *.hpp         # C++ headers
│       └── *.a           # Import libs for bcc64x
│
└── 370\                  # RAD Studio 13 compiled files
```

## 🔑 Registry Keys

| IDE Type | Registry Key |
|----------|--------------|
| 32-bit IDE | `HKCU\SOFTWARE\Embarcadero\BDS\{version}\Known Packages` |
| 64-bit IDE | `HKCU\SOFTWARE\Embarcadero\BDS\{version}\Known Packages x64` |

## 📝 Logging

Log files created next to executable / Лог-файлы создаются рядом с exe:
- Format: `DD_MM_YYYY_HH_MM.log`

---

## 📜 License / Лицензия

This software is distributed **free of charge** (Freeware).

Эта программа распространяется **бесплатно** (Freeware).

When redistributing, both authors must be credited:
При распространении обязательно указывать обоих авторов:

- **Original Delphi version**: [Delphier](https://github.com/Delphier/DxAutoInstaller)
- **C++ Edition**: Platon

## 👏 Credits & Acknowledgments

Special thanks to **Delphier** for the original Delphi version of DxAutoInstaller!

Огромная благодарность **Delphier** за оригинальную Delphi версию DxAutoInstaller!

The concept and architecture of this program are based on his work.

## 📧 Contact

**Platon**
- Email: vteme777@gmail.com
- GitHub: [@Platon7788](https://github.com/Platon7788)

## ☕ Support / Поддержка

If this program was useful and you want to buy me a coffee — I would be very grateful!

Если программа была полезна и вы хотите угостить меня кофе — буду очень признателен!

- **PayPal**: vteme777@gmail.com
- **Other methods / Другие способы**: write to email

---

**DxAutoInstaller C++ Edition v1.0.0** © 2025 Platon

Based on original work by [Delphier](https://github.com/Delphier/DxAutoInstaller)
