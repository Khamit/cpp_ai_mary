// core/AccessLevel.hpp
#pragma once

enum class AccessLevel {
    NONE = 0,        // 0 - нет доступа
    GUEST = 1,       // 1 - гость
    OPERATOR = 2,    // 2 - оператор
    EMPLOYEE = 3,    // 3 - сотрудник
    MANAGER = 4,     // 4 - менеджер
    ADMIN = 5,       // 5 - администратор
    MASTER = 6,      // 6 - мастер
    CREATOR = 7      // 7 - создатель
};