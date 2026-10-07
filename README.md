# Stella typechecker

Реализация обязательной части этапа 1 из `task`: C++20, BNFC, CMake.
Интерфейс проверки типов находится в `lib/checker.hpp`, реализация — в `lib/checker.cpp`, точка входа — в `src/main.cpp`, грамматика — в `Stella.cf`.
Парсер и AST генерируются в каталоге сборки.

## Сборка

Через Nix (сборка также запускает тесты):

```sh
nix build
./result/bin/typechecker < program.stella
```

`nix develop` открывает окружение с компилятором, CMake, BNFC, Bison,
Flex, Python и clang-format. Доступен также запуск через `nix run`.

Нужны C++20-компилятор, CMake >= 3.20, BNFC 2.9, Bison >= 3.0 и Flex.

```sh
cmake -S . -B build
cmake --build build -j
./build/typechecker < program.stella
```

Если инструменты не находятся в PATH, передайте их пути через
`-DBNFC_EXECUTABLE=/path/to/bnfc`, `-DBISON_EXECUTABLE=/path/to/bison`, `-DFLEX_EXECUTABLE=/path/to/flex` при конфигурации CMake.
На macOS системный Bison 2.3 не подходит.

Программа читает stdin. При успехе ничего не печатает и возвращает 0.
При ошибке печатает первое сообщение с кодом `ERROR_*` в stderr и возвращает 1.
Поддерживаются функции одного аргумента, Bool, Nat, Unit, кортежи,
записи, let, аннотации типов, суммы, варианты с данными, списки, Nat::rec и fix.
Директивы `extend with` разбираются; проверка включения расширений не выполняется.

## Проверка

```sh
python3 tests/check.py ./build/typechecker
```

Проверяются все 29 обязательных кодов ошибок и корректные программы
с обязательными конструкциями языка.
