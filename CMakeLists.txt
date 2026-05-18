cmake_minimum_required(VERSION 3.10)
project(Lab3 LANGUAGES CXX)

# Использовать стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Добавление исполняемого файла
add_executable(Lab3 Lab3.cpp)

# Настройка предупреждений компилятора
if(MSVC)
    target_compile_options(Lab3 PRIVATE /W4)
else()
    target_compile_options(Lab3 PRIVATE -Wall -Wextra -O2)
endif()

# Список текстовых файлов конфигурации и входных данных
set(DATA_FILES
    alphabet.txt
    delimiters.txt
    input.txt
    reserved_words.txt
)

# Копирование этих файлов в директорию сборки, чтобы исполняемый файл
# мог найти их при запуске из директории сборки
foreach(FILE ${DATA_FILES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
            "${CMAKE_CURRENT_BINARY_DIR}/${FILE}"
            COPYONLY
        )
    endif()
endforeach()
