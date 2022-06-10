#include "StringConverter.h"

std::wstring to_wstring(std::string str)
{
    std::wstring out;

    for (char& c : str)
    {
        wchar_t cw = wchar_t(0x0000 + c);
        out = out + cw;
    }

    return out;
}