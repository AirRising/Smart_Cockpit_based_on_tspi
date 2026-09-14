/****************************************************************************
** Meta object code from reading C++ file 'CarService.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/core/CarService.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CarService.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_sc__CarService_t {
    QByteArrayData data[36];
    char stringdata0[375];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_sc__CarService_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_sc__CarService_t qt_meta_stringdata_sc__CarService = {
    {
QT_MOC_LITERAL(0, 0, 14), // "sc::CarService"
QT_MOC_LITERAL(1, 15, 12), // "speedChanged"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 3), // "kmh"
QT_MOC_LITERAL(4, 33, 10), // "rpmChanged"
QT_MOC_LITERAL(5, 44, 3), // "rpm"
QT_MOC_LITERAL(6, 48, 11), // "fuelChanged"
QT_MOC_LITERAL(7, 60, 7), // "percent"
QT_MOC_LITERAL(8, 68, 17), // "indicatorsChanged"
QT_MOC_LITERAL(9, 86, 4), // "left"
QT_MOC_LITERAL(10, 91, 5), // "right"
QT_MOC_LITERAL(11, 97, 6), // "hazard"
QT_MOC_LITERAL(12, 104, 11), // "milsChanged"
QT_MOC_LITERAL(13, 116, 6), // "engine"
QT_MOC_LITERAL(14, 123, 3), // "abs"
QT_MOC_LITERAL(15, 127, 6), // "airbag"
QT_MOC_LITERAL(16, 134, 11), // "gearChanged"
QT_MOC_LITERAL(17, 146, 8), // "position"
QT_MOC_LITERAL(18, 155, 14), // "reverseChanged"
QT_MOC_LITERAL(19, 170, 6), // "active"
QT_MOC_LITERAL(20, 177, 20), // "steeringAngleChanged"
QT_MOC_LITERAL(21, 198, 7), // "degrees"
QT_MOC_LITERAL(22, 206, 19), // "climateStateChanged"
QT_MOC_LITERAL(23, 226, 16), // "sc::ClimateState"
QT_MOC_LITERAL(24, 243, 5), // "state"
QT_MOC_LITERAL(25, 249, 12), // "climateAckOk"
QT_MOC_LITERAL(26, 262, 9), // "requestId"
QT_MOC_LITERAL(27, 272, 17), // "climateAckTimeout"
QT_MOC_LITERAL(28, 290, 20), // "canConnectionChanged"
QT_MOC_LITERAL(29, 311, 9), // "connected"
QT_MOC_LITERAL(30, 321, 10), // "onCanFrame"
QT_MOC_LITERAL(31, 332, 2), // "id"
QT_MOC_LITERAL(32, 335, 4), // "data"
QT_MOC_LITERAL(33, 340, 8), // "extended"
QT_MOC_LITERAL(34, 349, 18), // "sendClimateCommand"
QT_MOC_LITERAL(35, 368, 6) // "target"

    },
    "sc::CarService\0speedChanged\0\0kmh\0"
    "rpmChanged\0rpm\0fuelChanged\0percent\0"
    "indicatorsChanged\0left\0right\0hazard\0"
    "milsChanged\0engine\0abs\0airbag\0gearChanged\0"
    "position\0reverseChanged\0active\0"
    "steeringAngleChanged\0degrees\0"
    "climateStateChanged\0sc::ClimateState\0"
    "state\0climateAckOk\0requestId\0"
    "climateAckTimeout\0canConnectionChanged\0"
    "connected\0onCanFrame\0id\0data\0extended\0"
    "sendClimateCommand\0target"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_sc__CarService[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      12,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   89,    2, 0x06 /* Public */,
       4,    1,   92,    2, 0x06 /* Public */,
       6,    1,   95,    2, 0x06 /* Public */,
       8,    3,   98,    2, 0x06 /* Public */,
      12,    3,  105,    2, 0x06 /* Public */,
      16,    1,  112,    2, 0x06 /* Public */,
      18,    1,  115,    2, 0x06 /* Public */,
      20,    1,  118,    2, 0x06 /* Public */,
      22,    1,  121,    2, 0x06 /* Public */,
      25,    1,  124,    2, 0x06 /* Public */,
      27,    1,  127,    2, 0x06 /* Public */,
      28,    1,  130,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      30,    3,  133,    2, 0x0a /* Public */,
      30,    2,  140,    2, 0x2a /* Public | MethodCloned */,
      34,    1,  145,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Double,    7,
    QMetaType::Void, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,    9,   10,   11,
    QMetaType::Void, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,   13,   14,   15,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void, QMetaType::Bool,   19,
    QMetaType::Void, QMetaType::Double,   21,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void, QMetaType::UInt,   26,
    QMetaType::Void, QMetaType::UInt,   26,
    QMetaType::Void, QMetaType::Bool,   29,

 // slots: parameters
    QMetaType::Void, QMetaType::UInt, QMetaType::QByteArray, QMetaType::Bool,   31,   32,   33,
    QMetaType::Void, QMetaType::UInt, QMetaType::QByteArray,   31,   32,
    QMetaType::UInt, 0x80000000 | 23,   35,

       0        // eod
};

void sc::CarService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CarService *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->speedChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 1: _t->rpmChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->fuelChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 3: _t->indicatorsChanged((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 4: _t->milsChanged((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 5: _t->gearChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->reverseChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->steeringAngleChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 8: _t->climateStateChanged((*reinterpret_cast< const sc::ClimateState(*)>(_a[1]))); break;
        case 9: _t->climateAckOk((*reinterpret_cast< quint32(*)>(_a[1]))); break;
        case 10: _t->climateAckTimeout((*reinterpret_cast< quint32(*)>(_a[1]))); break;
        case 11: _t->canConnectionChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 12: _t->onCanFrame((*reinterpret_cast< quint32(*)>(_a[1])),(*reinterpret_cast< const QByteArray(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 13: _t->onCanFrame((*reinterpret_cast< quint32(*)>(_a[1])),(*reinterpret_cast< const QByteArray(*)>(_a[2]))); break;
        case 14: { quint32 _r = _t->sendClimateCommand((*reinterpret_cast< const sc::ClimateState(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< quint32*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< sc::ClimateState >(); break;
            }
            break;
        case 14:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< sc::ClimateState >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CarService::*)(double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::speedChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CarService::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::rpmChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (CarService::*)(double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::fuelChanged)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (CarService::*)(bool , bool , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::indicatorsChanged)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (CarService::*)(bool , bool , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::milsChanged)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (CarService::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::gearChanged)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (CarService::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::reverseChanged)) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (CarService::*)(double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::steeringAngleChanged)) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (CarService::*)(const sc::ClimateState & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::climateStateChanged)) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (CarService::*)(quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::climateAckOk)) {
                *result = 9;
                return;
            }
        }
        {
            using _t = void (CarService::*)(quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::climateAckTimeout)) {
                *result = 10;
                return;
            }
        }
        {
            using _t = void (CarService::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CarService::canConnectionChanged)) {
                *result = 11;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject sc::CarService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_sc__CarService.data,
    qt_meta_data_sc__CarService,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *sc::CarService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *sc::CarService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_sc__CarService.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int sc::CarService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void sc::CarService::speedChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void sc::CarService::rpmChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void sc::CarService::fuelChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void sc::CarService::indicatorsChanged(bool _t1, bool _t2, bool _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void sc::CarService::milsChanged(bool _t1, bool _t2, bool _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void sc::CarService::gearChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void sc::CarService::reverseChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void sc::CarService::steeringAngleChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void sc::CarService::climateStateChanged(const sc::ClimateState & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void sc::CarService::climateAckOk(quint32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void sc::CarService::climateAckTimeout(quint32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void sc::CarService::canConnectionChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
