/****************************************************************************
** Meta object code from reading C++ file 'MediaPage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/ui/MediaPage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MediaPage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_sc__MediaPage_t {
    QByteArrayData data[12];
    char stringdata0[153];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_sc__MediaPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_sc__MediaPage_t qt_meta_stringdata_sc__MediaPage = {
    {
QT_MOC_LITERAL(0, 0, 13), // "sc::MediaPage"
QT_MOC_LITERAL(1, 14, 12), // "openSelected"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 17), // "onPositionChanged"
QT_MOC_LITERAL(4, 46, 10), // "positionMs"
QT_MOC_LITERAL(5, 57, 17), // "onDurationChanged"
QT_MOC_LITERAL(6, 75, 10), // "durationMs"
QT_MOC_LITERAL(7, 86, 22), // "onPlaybackStateChanged"
QT_MOC_LITERAL(8, 109, 7), // "playing"
QT_MOC_LITERAL(9, 117, 14), // "onTrackChanged"
QT_MOC_LITERAL(10, 132, 5), // "title"
QT_MOC_LITERAL(11, 138, 14) // "onSeekReleased"

    },
    "sc::MediaPage\0openSelected\0\0"
    "onPositionChanged\0positionMs\0"
    "onDurationChanged\0durationMs\0"
    "onPlaybackStateChanged\0playing\0"
    "onTrackChanged\0title\0onSeekReleased"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_sc__MediaPage[] = {

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
       1,    0,   44,    2, 0x08 /* Private */,
       3,    1,   45,    2, 0x08 /* Private */,
       5,    1,   48,    2, 0x08 /* Private */,
       7,    1,   51,    2, 0x08 /* Private */,
       9,    1,   54,    2, 0x08 /* Private */,
      11,    0,   57,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong,    4,
    QMetaType::Void, QMetaType::LongLong,    6,
    QMetaType::Void, QMetaType::Bool,    8,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void,

       0        // eod
};

void sc::MediaPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MediaPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->openSelected(); break;
        case 1: _t->onPositionChanged((*reinterpret_cast< qint64(*)>(_a[1]))); break;
        case 2: _t->onDurationChanged((*reinterpret_cast< qint64(*)>(_a[1]))); break;
        case 3: _t->onPlaybackStateChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->onTrackChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 5: _t->onSeekReleased(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject sc::MediaPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_sc__MediaPage.data,
    qt_meta_data_sc__MediaPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *sc::MediaPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *sc::MediaPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_sc__MediaPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int sc::MediaPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
