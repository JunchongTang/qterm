#pragma once

#include <QtGlobal>

struct QTermCell
{
    quint32 code = 0;
    quint32 foreground = 0xffd0d0d0;
    quint32 background = 0xff101010;
    quint16 attributes = 0;
    quint8 width = 1;
};