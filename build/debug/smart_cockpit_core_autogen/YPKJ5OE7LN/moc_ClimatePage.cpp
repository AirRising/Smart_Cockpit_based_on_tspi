/****************************************************************************
** Meta object code from reading C++ file 'ClimatePage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/ui/ClimatePage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ClimatePage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_sc__ClimatePage_t {
    QByteArrayData data[9];
    char stringdata0[110];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_sc__ClimatePage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_sc__ClimatePage_t qt_meta_stringdata_sc__ClimatePage = {
    {
QT_MOC_LITERAL(0, 0, 15), // "sc::ClimatePage"
QT_MOC_LITERAL(1, 16, 16), // "onControlChanged"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 21), // "onClimateStateChanged"
QT_MOC_LITERAL(4, 56, 16), // "sc::ClimateState"
QT_MOC_LITERAL(5, 73, 5), // "state"
QT_MOC_LITERAL(6, 79, 7), // "onAckOk"
QT_MOC_LITERAL(7, 87, 9), // "requestId"
QT_MOC_LITERAL(8, 97, 12) // "onAckTimeout"

    },
    "sc::ClimatePage\0onControlChanged\0\0"
    "onClimateStateChanged\0sc::ClimateState\0"
    "state\0onAckOk\0requestId\0onAckTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_sc__ClimatePage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   34,    2, 0x08 /* Private */,
       3,    1,   35,    2, 0x08 /* Private */,
       6,    1,   38,    2, 0x08 /* Private */,
       8,    1,   41,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, QMetaType::UInt,    7,
    QMetaType::Void, QMetaType::UInt,    7,

       0        // eod
};

void sc::ClimatePage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ClimatePage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onControlChanged(); break;
        case 1: _t->onClimateStateChanged((*reinterpret_cast< const sc::ClimateState(*)>(_a[1]))); break;
        case 2: _t->onAckOk((*reinterpret_cast< quint32(*)>(_a[1]))); break;
        case 3: _t->onAckTimeout((*reinterpret_cast< quint32(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< sc::ClimateState >(); break;
            }
            break;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject sc::ClimatePage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_sc__ClimatePage.data,
    qt_meta_data_sc__ClimatePage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *sc::ClimatePage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *sc::ClimatePage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_sc__ClimatePage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int sc::ClimatePage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
