#pragma once

#include <string>

// Под хостом Arduino String — это std::string.
//
// Так сделано намеренно: ArduinoJson поддерживает std::string из коробки
// (ARDUINOJSON_ENABLE_STD_STRING включен по умолчанию вне Arduino), поэтому
// ConfigManager и AlertManager с их to_json()/from_json() собираются
// без единой правки. Своя реализация String потребовала бы точного совпадения
// с внутренним адаптером ArduinoJson и ломалась бы при обновлении библиотеки
using String = std::string;
