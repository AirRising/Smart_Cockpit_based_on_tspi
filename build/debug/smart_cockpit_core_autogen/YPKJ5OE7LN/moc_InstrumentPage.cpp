/****************************************************************************
** Meta object code from reading C++ file 'InstrumentPage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/ui/InstrumentPage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'InstrumentPage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_sc__InstrumentPage_t {
    QByteArrayData data[18];
    char stringdata0[157];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_sc__InstrumentPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_sc__InstrumentPage_t qt_meta_stringdata_sc__InstrumentPage = {
    {
QT_MOC_LITERAL(0, 0, 18), // "sc::InstrumentPage"
QT_MOC_LITERAL(1, 19, 14), // "onSpeedChanged"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 3), // "kmh"
QT_MOC_LITERAL(4, 39, 12), // "onRpmChanged"
QT_MOC_LITERAL(5, 52, 3), // "rpm"
QT_MOC_LITERAL(6, 56, 13), // "onFuelChanged"
QT_MOC_LITERAL(7, 70, 7), // "percent"
QT_MOC_LITERAL(8, 78, 12), // "onIndicators"
QT_MOC_LITERAL(9, 91, 4), // "left"
QT_MOC_LITERAL(10, 96, 5), // "right"
QT_MOC_LITERAL(11, 102, 6), // "hazard"
QT_MOC_LITERAL(12, 109, 6), // "onMils"
QT_MOC_LITERAL(13, 116, 6), // "engine"
QT_MOC_LITERAL(14, 123, 3), // "abs"
QT_MOC_LITERAL(15, 127, 6), // "airbag"
QT_MOC_LITERAL(16, 134, 13), // "onGearChanged"
QT_MOC_LITERAL(17, 148, 8) // "position"

    },
    "sc::InstrumentPage\0onSpeedChanged\0\0"
    "kmh\0onRpmChanged\0rpm\0onFuelChanged\0"
    "percent\0onIndicators\0left\0right\0hazard\0"
    "onMils\0engine\0abs\0airbag\0onGearChanged\0"
    "position"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_sc__InstrumentPage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   44,    2, 0x08 /* Private */,
       4,    1,   47,    2, 0x08 /* Private */,
       6,    1,   50,    2, 0x08 /* Private */,
       8,    3,   53,    2, 0x08 /* Private */,
      12,    3,   60,    2, 0x08 /* Private */,
      16,    1,   67,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Double,    7,
    QMetaType::Void, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,    9,   10,   11,
    QMetaType::Void, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,   13,   14,   15,
    QMetaType::Void, QMetaType::Int,   17,

       0        // eod
};

void sc::InstrumentPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InstrumentPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSpeedChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 1: _t->onRpmChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onFuelChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 3: _t->onIndicators((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 4: _t->onMils((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 5: _t->onGearChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject sc::InstrumentPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_sc__InstrumentPage.data,
    qt_meta_data_sc__InstrumentPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *sc::InstrumentPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *sc::InstrumentPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_sc__InstrumentPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int sc::InstrumentPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
