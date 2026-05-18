cmake_minimum_required(VERSION 3.10)
project(Lab3 LANGUAGES CXX)

# Использовать стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Добавление исполняемого файла
add_executable(Lab3 
    main.cpp
    LexerTypes.cpp
    Utils.cpp
    Tables.cpp
    Lexer.cpp
    Parser.cpp
    Interpreter.cpp
)

# Настройка предупреждений компилятора
if(MSVC)
    target_compile_options(Lab3 PRIVATE /W4)
else()
    target_compile_options(Lab3 PRIVATE -Wall -Wextra -O2)
endif()

# Копирование директорий конфигурации и данных в директорию сборки
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# Создание папки для выходных файлов
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
