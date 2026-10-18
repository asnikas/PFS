#!/usr/bin/env python3
"""
Запускалка сценариев PFS.

Сценарий — пара файлов в tests/scenarios/:
    имя.txt      команды PFS, по одной на строку
    имя.expect   проверки того, что PFS напечатала

Формат .txt:
    # комментарий        строка пропускается
    --- restart ---      завершить программу и запустить её заново;
                         образ сохраняется (проверка персистентности)
    пустые строки пропускаются

Формат .expect:
    contains: текст      текст должен встретиться в выводе. Проверки contains
                         идут ПО ПОРЯДКУ: каждая ищется после того места,
                         где нашлась предыдущая
    not_contains: текст  текста не должно быть нигде в выводе
    # комментарий

Перед каждым запуском в общий вывод вставляется строка "=== run N ===",
её тоже можно искать через contains (например, чтобы проверить, что после
перезапуска файл читается).

Каждый сценарий выполняется в отдельной временной папке, поэтому disk.img
всегда создаётся с нуля и не мешает другим сценариям.

Запуск (из корня репозитория):
    python tests/run_tests.py               собрать main.cpp и прогнать все сценарии
    python tests/run_tests.py basic perms   только сценарии с этими именами
    python tests/run_tests.py -v            печатать вывод и у успешных сценариев
    python tests/run_tests.py --exe путь    не собирать, взять готовый исполняемый файл
"""

import argparse
import os
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent      # корень репозитория
DEFAULT_SCENARIOS = ROOT / "tests" / "scenarios"
TIMEOUT = 10                                        # секунд на один запуск: защита от зацикливания
RESTART = "--- restart ---"


def build():
    """Собирает все .cpp из корня в build/pfs(.exe) и возвращает путь к исполняемому файлу.

    Компилятор вызывается напрямую, без CMake, чтобы тесты запускались и там, где CMake не установлен.
    """
    exe = ROOT / "build" / ("pfs.exe" if os.name == "nt" else "pfs")
    exe.parent.mkdir(exist_ok=True)
    sources = sorted(str(path) for path in ROOT.glob("*.cpp"))
    cmd = ["g++", "-std=c++17", "-g", "-o", str(exe)] + sources
    if os.name == "nt":
        # без этого exe ищет DLL MinGW в PATH и может не запуститься
        cmd.append("-static")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("сборка не удалась:")
        print(result.stderr)
        sys.exit(2)
    return exe


def read_scenario(path):
    """Возвращает список запусков; каждый запуск — список команд."""
    runs = [[]]
    # utf-8-sig убирает невидимую метку BOM, которую добавляют Блокнот и PowerShell
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped == RESTART:
            runs.append([])
        else:
            runs[-1].append(line)
    return runs


def read_expect(path):
    """Возвращает список проверок вида ("contains" | "not_contains", текст)."""
    checks = []
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        kind, sep, text = stripped.partition(":")
        kind = kind.strip()
        if not sep or kind not in ("contains", "not_contains"):
            raise ValueError(f"{path.name}, строка {number}: непонятная проверка «{line}»")
        checks.append((kind, text.strip()))
    return checks


def run_scenario(exe, runs):
    """Выполняет все запуски сценария в одной временной папке.

    Возвращает (весь вывод, описание аварии или None).
    """
    output = []
    with tempfile.TemporaryDirectory() as workdir:
        for number, commands in enumerate(runs, 1):
            output.append(f"=== run {number} ===")
            stdin = "".join(command + "\n" for command in commands).encode("utf-8")
            try:
                result = subprocess.run(
                    [str(exe)],
                    input=stdin,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,   # ошибки в тот же поток, чтобы порядок сохранился
                    cwd=workdir,                 # disk.img создаётся во временной папке
                    timeout=TIMEOUT,
                )
            except subprocess.TimeoutExpired:
                return "\n".join(output), f"запуск {number}: завис (дольше {TIMEOUT} с)"
            output.append(result.stdout.decode("utf-8", errors="replace").rstrip("\n"))
            if result.returncode != 0:
                code = result.returncode
                # на Windows аварии видны как большие коды, например 0xC0000005 — обращение к чужой памяти
                hex_code = f" (0x{code & 0xFFFFFFFF:08X})" if os.name == "nt" else ""
                return "\n".join(output), f"запуск {number}: программа завершилась с кодом {code}{hex_code}"
    return "\n".join(output), None


def check(output, checks):
    """Возвращает список описаний проваленных проверок (пустой, если всё хорошо)."""
    errors = []
    position = 0                                    # откуда искать следующий contains
    for kind, text in checks:
        if kind == "contains":
            found = output.find(text, position)
            if found == -1:
                where = " (после предыдущей найденной проверки)" if position > 0 else ""
                errors.append(f"не найдено «{text}»{where}")
            else:
                position = found + len(text)
        elif text in output:
            errors.append(f"найдено «{text}», а не должно")
    return errors


def main():
    parser = argparse.ArgumentParser(description="Прогон сценариев PFS")
    parser.add_argument("names", nargs="*", help="имена сценариев без расширения (по умолчанию все)")
    parser.add_argument("--exe", help="готовый исполняемый файл PFS (тогда сборки нет)")
    parser.add_argument("--dir", default=str(DEFAULT_SCENARIOS), help="папка со сценариями")
    parser.add_argument("-v", "--verbose", action="store_true", help="печатать вывод и у успешных сценариев")
    args = parser.parse_args()

    exe = Path(args.exe).resolve() if args.exe else build()

    scenarios = sorted(Path(args.dir).glob("*.txt"))
    if args.names:
        scenarios = [path for path in scenarios if path.stem in args.names]
    if not scenarios:
        print("сценарии не найдены")
        return 2

    failed = 0
    for txt in scenarios:
        expect = txt.with_suffix(".expect")
        if not expect.exists():
            print(f"FAIL {txt.stem}: нет файла {expect.name}")
            failed += 1
            continue
        try:
            checks = read_expect(expect)
        except ValueError as error:
            print(f"FAIL {txt.stem}: {error}")
            failed += 1
            continue

        output, crash = run_scenario(exe, read_scenario(txt))
        errors = ([crash] if crash else []) + check(output, checks)

        if errors:
            failed += 1
            print(f"FAIL {txt.stem}")
            for error in errors:
                print(f"    - {error}")
            print(textwrap.indent(output, "    | "))
        else:
            print(f"PASS {txt.stem}")
            if args.verbose:
                print(textwrap.indent(output, "    | "))

    print(f"\n{len(scenarios) - failed} из {len(scenarios)} прошли")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
