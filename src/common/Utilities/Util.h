/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UTIL_H
#define _UTIL_H

#include "Define.h"
#include <array>
#include <string>
#include <string_view>

WH_COMMON_API bool StringEqualI(std::string_view str1, std::string_view str2);
WH_COMMON_API std::string GetLowerString(std::string_view str);

namespace Warhead::Impl
{
    WH_COMMON_API std::string ByteArrayToHexStr(uint8 const* bytes, size_t length, bool reverse = false);
    WH_COMMON_API void HexStrToByteArray(std::string_view str, uint8* out, size_t outlen, bool reverse = false);
}

template<typename Container>
std::string ByteArrayToHexStr(Container const& c, bool reverse = false)
{
    return Warhead::Impl::ByteArrayToHexStr(std::data(c), std::size(c), reverse);
}

template<size_t Size>
void HexStrToByteArray(std::string_view str, std::array<uint8, Size>& buf, bool reverse = false)
{
    Warhead::Impl::HexStrToByteArray(str, buf.data(), Size, reverse);
}

template<size_t Size>
std::array<uint8, Size> HexStrToByteArray(std::string_view str, bool reverse = false)
{
    std::array<uint8, Size> arr;
    HexStrToByteArray(str, arr, reverse);
    return arr;
}

// set string to "" if invalid utf8 sequence
WH_COMMON_API size_t utf8length(std::string& utf8str);
WH_COMMON_API void utf8truncate(std::string& utf8str, size_t len);

WH_COMMON_API bool Utf8ToUpperOnlyLatin(std::string& utf8String);

// UTF8 handling
WH_COMMON_API bool Utf8toWStr(std::string_view utf8str, std::wstring& wstr);

// in wsize == max size of buffer, out wsize==real string size
WH_COMMON_API bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize);

WH_COMMON_API bool WStrToUtf8(std::wstring_view wstr, std::string& utf8str);

inline bool isNumeric(char c)
{
    return (c >= '0' && c <= '9');
}

inline bool isNumericOrSpace(wchar_t wchar)
{
    return isNumeric(wchar) || wchar == L' ';
}

inline bool isBasicLatinCharacter(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z') // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
    {
        return true;
    }

    if (wchar >= L'A' && wchar <= L'Z') // LATIN CAPITAL LETTER A - LATIN CAPITAL LETTER Z
    {
        return true;
    }

    return false;
}

inline bool isBasicLatinString(std::wstring_view wstr, bool numericOrSpace)
{
    for (wchar_t i : wstr)
    {
        if (!isBasicLatinCharacter(i) && (!numericOrSpace || !isNumericOrSpace(i)))
        {
            return false;
        }
    }

    return true;
}

inline wchar_t wcharToUpper(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z')                      // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
        return wchar_t(uint16(wchar) - 0x0020);

    if (wchar == 0x00DF)                                     // LATIN SMALL LETTER SHARP S
        return wchar_t(0x1E9E);

    if (wchar >= 0x00E0 && wchar <= 0x00F6)                  // LATIN SMALL LETTER A WITH GRAVE - LATIN SMALL LETTER O WITH DIAERESIS
        return wchar_t(uint16(wchar) - 0x0020);

    if (wchar >= 0x00F8 && wchar <= 0x00FE)                  // LATIN SMALL LETTER O WITH STROKE - LATIN SMALL LETTER THORN
        return wchar_t(uint16(wchar) - 0x0020);

    if (wchar >= 0x0101 && wchar <= 0x012F)                  // LATIN SMALL LETTER A WITH MACRON - LATIN SMALL LETTER I WITH OGONEK (only %2=1)
        if (wchar % 2 == 1)
            return wchar_t(uint16(wchar) - 0x0001);

    if (wchar >= 0x0430 && wchar <= 0x044F)                  // CYRILLIC SMALL LETTER A - CYRILLIC SMALL LETTER YA
        return wchar_t(uint16(wchar) - 0x0020);

    if (wchar == 0x0451)                                     // CYRILLIC SMALL LETTER IO
        return wchar_t(0x0401);

    return wchar;
}

inline wchar_t wcharToUpperOnlyLatin(wchar_t wchar)
{
    return isBasicLatinCharacter(wchar) ? wcharToUpper(wchar) : wchar;
}

#endif
