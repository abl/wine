/*
 * Unit test suite for crypt32.dll's CryptEncodeObjectEx/CryptDecodeObjectEx
 *
 * Copyright 2005 Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <wincrypt.h>

#include "wine/test.h"

struct encodedInt
{
    int val;
    BYTE *encoded;
};

static const struct encodedInt ints[] = {
 { 1,          "\x02\x01\x01" },
 { 127,        "\x02\x01\x7f" },
 { 128,        "\x02\x02\x00\x80" },
 { 256,        "\x02\x02\x01\x00" },
 { -128,       "\x02\x01\x80" },
 { -129,       "\x02\x02\xff\x7f" },
 { 0xbaddf00d, "\x02\x04\xba\xdd\xf0\x0d" },
};

struct encodedBigInt
{
    BYTE *val;
    BYTE *encoded;
    BYTE *decoded;
};

static const struct encodedBigInt bigInts[] = {
 { "\xff\xff\x01\x02\x03\x04\x05\x06\x07\x08",
   "\x02\x0a\x08\x07\x06\x05\x04\x03\x02\x01\xff\xff",
   "\xff\xff\x01\x02\x03\x04\x05\x06\x07\x08" },
 { "\x08\x07\x06\x05\x04\x03\x02\x01\xff\xff\xff",
   "\x02\x09\xff\x01\x02\x03\x04\x05\x06\x07\x08",
   "\x08\x07\x06\x05\x04\x03\x02\x01\xff" },
};

/* Decoded is the same as original, so don't bother storing a separate copy */
static const struct encodedBigInt bigUInts[] = {
 { "\xff\xff\x01\x02\x03\x04\x05\x06\x07\x08",
   "\x02\x0a\x08\x07\x06\x05\x04\x03\x02\x01\xff\xff", NULL },
 { "\x08\x07\x06\x05\x04\x03\x02\x01\xff\xff\xff",
   "\x02\x0c\x00\xff\xff\xff\x01\x02\x03\x04\x05\x06\x07\x08", NULL },
};

static void test_encodeInt(DWORD dwEncoding)
{
    DWORD bufSize = 0;
    int i;
    BOOL ret;
    CRYPT_INTEGER_BLOB blob;
    BYTE *buf = NULL;

    /* CryptEncodeObjectEx with NULL bufSize crashes..
    ret = CryptEncodeObjectEx(3, X509_INTEGER, &ints[0].val, 0, NULL, NULL,
     NULL);
     */
    /* check bogus encoding */
    ret = CryptEncodeObjectEx(0, X509_INTEGER, &ints[0].val, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %ld\n", GetLastError());
    /* check with NULL integer buffer.  Windows XP incorrectly returns an
     * NTSTATUS.
     */
    ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, NULL, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
    {
        /* encode as normal integer */
        ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, &ints[i].val, 0,
         NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, &ints[i].val,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(!memcmp(buf + 1, ints[i].encoded + 1, ints[i].encoded[1] + 1),
             "Encoded value of 0x%08x didn't match expected\n", ints[i].val);
            LocalFree(buf);
        }
        /* encode as multibyte integer */
        blob.cbData = sizeof(ints[i].val);
        blob.pbData = (BYTE *)&ints[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == ints[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], ints[i].encoded[1]);
            ok(!memcmp(buf + 1, ints[i].encoded + 1, ints[i].encoded[1] + 1),
             "Encoded value of 0x%08x didn't match expected\n", ints[i].val);
            LocalFree(buf);
        }
    }
    /* encode a couple bigger ints, just to show it's little-endian and leading
     * sign bytes are dropped
     */
    for (i = 0; i < sizeof(bigInts) / sizeof(bigInts[0]); i++)
    {
        blob.cbData = strlen(bigInts[i].val);
        blob.pbData = (BYTE *)bigInts[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == bigInts[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], bigInts[i].encoded[1]);
            ok(!memcmp(buf + 1, bigInts[i].encoded + 1,
             bigInts[i].encoded[1] + 1),
             "Encoded value didn't match expected\n");
            LocalFree(buf);
        }
    }
    /* and, encode some uints */
    for (i = 0; i < sizeof(bigUInts) / sizeof(bigUInts[0]); i++)
    {
        blob.cbData = strlen(bigUInts[i].val);
        blob.pbData = (BYTE *)bigUInts[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == bigUInts[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], bigUInts[i].encoded[1]);
            ok(!memcmp(buf + 1, bigUInts[i].encoded + 1,
             bigUInts[i].encoded[1] + 1),
             "Encoded value didn't match expected\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeInt(DWORD dwEncoding)
{
    static const char bigInt[] = { 2, 5, 0xff, 0xfe, 0xff, 0xfe, 0xff };
    static const char testStr[] = { 0x16, 4, 't', 'e', 's', 't' };
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    int i;
    BOOL ret;

    /* CryptDecodeObjectEx with NULL bufSize crashes..
    ret = CryptDecodeObjectEx(3, X509_INTEGER, &ints[0].encoded, 
     ints[0].encoded[1] + 2, 0, NULL, NULL, NULL);
     */
    /* check bogus encoding */
    ret = CryptDecodeObjectEx(3, X509_INTEGER, (BYTE *)&ints[0].encoded, 
     ints[0].encoded[1] + 2, 0, NULL, NULL, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %ld\n", GetLastError());
    /* check with NULL integer buffer */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, NULL, 0, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_EOD,
     "Expected CRYPT_E_ASN1_EOD, got %08lx\n", GetLastError());
    /* check with a valid, but too large, integer */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, bigInt, bigInt[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_LARGE,
     "Expected CRYPT_E_ASN1_LARGE, got %ld\n", GetLastError());
    /* check with a DER-encoded string */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, testStr, testStr[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_BADTAG,
     "Expected CRYPT_E_ASN1_BADTAG, got %ld\n", GetLastError());
    for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
    {
        /* When the output buffer is NULL, this always succeeds */
        SetLastError(0xdeadbeef);
        ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER,
         (BYTE *)ints[i].encoded, ints[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER,
         (BYTE *)ints[i].encoded, ints[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize == sizeof(int), "Expected size %d, got %ld\n", sizeof(int),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            ok(!memcmp(buf, &ints[i].val, bufSize), "Expected %d, got %d\n",
             ints[i].val, *(int *)buf);
            LocalFree(buf);
        }
    }
    for (i = 0; i < sizeof(bigInts) / sizeof(bigInts[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER,
         (BYTE *)bigInts[i].encoded, bigInts[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER,
         (BYTE *)bigInts[i].encoded, bigInts[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_INTEGER_BLOB),
         "Expected size at least %d, got %ld\n", sizeof(CRYPT_INTEGER_BLOB),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_INTEGER_BLOB *blob = (CRYPT_INTEGER_BLOB *)buf;

            ok(blob->cbData == strlen(bigInts[i].decoded),
             "Expected len %d, got %ld\n", strlen(bigInts[i].decoded),
             blob->cbData);
            ok(!memcmp(blob->pbData, bigInts[i].decoded, blob->cbData),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
    for (i = 0; i < sizeof(bigUInts) / sizeof(bigUInts[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT,
         (BYTE *)bigUInts[i].encoded, bigUInts[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT,
         (BYTE *)bigUInts[i].encoded, bigUInts[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_INTEGER_BLOB),
         "Expected size at least %d, got %ld\n", sizeof(CRYPT_INTEGER_BLOB),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_INTEGER_BLOB *blob = (CRYPT_INTEGER_BLOB *)buf;

            ok(blob->cbData == strlen(bigUInts[i].val),
             "Expected len %d, got %ld\n", strlen(bigUInts[i].val),
             blob->cbData);
            ok(!memcmp(blob->pbData, bigUInts[i].val, blob->cbData),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

/* These are always encoded unsigned, and aren't constrained to be any
 * particular value
 */
static const struct encodedInt enums[] = {
 { 1,    "\x0a\x01\x01" },
 { -128, "\x0a\x05\x00\xff\xff\xff\x80" },
};

/* X509_CRL_REASON_CODE is also an enumerated type, but it's #defined to
 * X509_ENUMERATED.
 */
static const LPCSTR enumeratedTypes[] = { X509_ENUMERATED,
 szOID_CRL_REASON_CODE };

static void test_encodeEnumerated(DWORD dwEncoding)
{
    DWORD i, j;

    for (i = 0; i < sizeof(enumeratedTypes) / sizeof(enumeratedTypes[0]); i++)
    {
        for (j = 0; j < sizeof(enums) / sizeof(enums[0]); j++)
        {
            BOOL ret;
            BYTE *buf = NULL;
            DWORD bufSize = 0;

            ret = CryptEncodeObjectEx(dwEncoding, enumeratedTypes[i],
             &enums[j].val, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
             &bufSize);
            ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
            if (buf)
            {
                ok(buf[0] == 0xa,
                 "Got unexpected type %d for enumerated (expected 0xa)\n",
                 buf[0]);
                ok(buf[1] == enums[j].encoded[1],
                 "Got length %d, expected %d\n", buf[1], enums[j].encoded[1]);
                ok(!memcmp(buf + 1, enums[j].encoded + 1,
                 enums[j].encoded[1] + 1),
                 "Encoded value of 0x%08x didn't match expected\n",
                 enums[j].val);
                LocalFree(buf);
            }
        }
    }
}

static void test_decodeEnumerated(DWORD dwEncoding)
{
    DWORD i, j;

    for (i = 0; i < sizeof(enumeratedTypes) / sizeof(enumeratedTypes[0]); i++)
    {
        for (j = 0; j < sizeof(enums) / sizeof(enums[0]); j++)
        {
            BOOL ret;
            DWORD bufSize = sizeof(int);
            int val;

            ret = CryptDecodeObjectEx(dwEncoding, enumeratedTypes[i],
             enums[j].encoded, enums[j].encoded[1] + 2, 0, NULL,
             (BYTE *)&val, &bufSize);
            ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
            ok(bufSize == sizeof(int),
             "Got unexpected size %ld for enumerated (expected %d)\n",
             bufSize, sizeof(int));
            ok(val == enums[j].val, "Unexpected value %d, expected %d\n",
             val, enums[j].val);
        }
    }
}

struct encodedFiletime
{
    SYSTEMTIME sysTime;
    const BYTE *encodedTime;
};

static void testTimeEncoding(DWORD dwEncoding, LPCSTR structType,
 const struct encodedFiletime *time)
{
    FILETIME ft = { 0 };
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    BOOL ret;

    ret = SystemTimeToFileTime(&time->sysTime, &ft);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    ret = CryptEncodeObjectEx(dwEncoding, structType, &ft,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    /* years other than 1950-2050 are not allowed for encodings other than
     * X509_CHOICE_OF_TIME.
     */
    if (structType == X509_CHOICE_OF_TIME ||
     (time->sysTime.wYear >= 1950 && time->sysTime.wYear <= 2050))
    {
        ok(ret, "CryptEncodeObjectEx failed: %ld (0x%08lx)\n", GetLastError(),
         GetLastError());
        ok(buf != NULL, "Expected an allocated buffer\n");
        if (buf)
        {
            ok(buf[0] == time->encodedTime[0],
             "Expected type 0x%02x, got 0x%02x\n", time->encodedTime[0],
             buf[0]);
            ok(buf[1] == time->encodedTime[1], "Expected %d bytes, got %ld\n",
             time->encodedTime[1], bufSize);
            ok(!memcmp(time->encodedTime + 2, buf + 2, time->encodedTime[1]),
             "Got unexpected value for time encoding\n");
            LocalFree(buf);
        }
    }
    else
        ok(!ret && GetLastError() == CRYPT_E_BAD_ENCODE,
         "Expected CRYPT_E_BAD_ENCODE, got 0x%08lx\n", GetLastError());
}

static void testTimeDecoding(DWORD dwEncoding, LPCSTR structType,
 const struct encodedFiletime *time)
{
    FILETIME ft1 = { 0 }, ft2 = { 0 };
    DWORD size = sizeof(ft2);
    BOOL ret;

    ret = SystemTimeToFileTime(&time->sysTime, &ft1);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    ret = CryptDecodeObjectEx(dwEncoding, structType, time->encodedTime,
     time->encodedTime[1] + 2, 0, NULL, &ft2, &size);
    /* years other than 1950-2050 are not allowed for encodings other than
     * X509_CHOICE_OF_TIME.
     */
    if (structType == X509_CHOICE_OF_TIME ||
     (time->sysTime.wYear >= 1950 && time->sysTime.wYear <= 2050))
    {
        ok(ret, "CryptDecodeObjectEx failed: %ld (0x%08lx)\n", GetLastError(),
         GetLastError());
        ok(!memcmp(&ft1, &ft2, sizeof(ft1)),
         "Got unexpected value for time decoding\n");
    }
    else
        ok(!ret && GetLastError() == CRYPT_E_ASN1_BADTAG,
         "Expected CRYPT_E_ASN1_BADTAG, got 0x%08lx\n", GetLastError());
}

static const struct encodedFiletime times[] = {
 { { 2005, 6, 1, 6, 16, 10, 0, 0 }, "\x17" "\x0d" "050606161000Z" },
 { { 1945, 6, 1, 6, 16, 10, 0, 0 }, "\x18" "\x0f" "19450606161000Z" },
 { { 2145, 6, 1, 6, 16, 10, 0, 0 }, "\x18" "\x0f" "21450606161000Z" },
};

static void test_encodeFiletime(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(times) / sizeof(times[0]); i++)
    {
        testTimeEncoding(dwEncoding, X509_CHOICE_OF_TIME, &times[i]);
        testTimeEncoding(dwEncoding, PKCS_UTC_TIME, &times[i]);
        testTimeEncoding(dwEncoding, szOID_RSA_signingTime, &times[i]);
    }
}

static void test_decodeFiletime(DWORD dwEncoding)
{
    static const struct encodedFiletime otherTimes[] = {
     { { 1945, 6, 1, 6, 16, 10, 0, 0 },   "\x18" "\x13" "19450606161000.000Z" },
     { { 1945, 6, 1, 6, 16, 10, 0, 999 }, "\x18" "\x13" "19450606161000.999Z" },
     { { 1945, 6, 1, 6, 17, 10, 0, 0 },   "\x18" "\x13" "19450606161000+0100" },
     { { 1945, 6, 1, 6, 15, 10, 0, 0 },   "\x18" "\x13" "19450606161000-0100" },
     { { 1945, 6, 1, 6, 14, 55, 0, 0 },   "\x18" "\x13" "19450606161000-0115" },
     { { 2145, 6, 1, 6, 16,  0, 0, 0 },   "\x18" "\x0a" "2145060616" },
     { { 2045, 6, 1, 6, 16, 10, 0, 0 },   "\x17" "\x0a" "4506061610" },
     { { 2045, 6, 1, 6, 16, 10, 0, 0 },   "\x17" "\x0b" "4506061610Z" },
     { { 2045, 6, 1, 6, 17, 10, 0, 0 },   "\x17" "\x0d" "4506061610+01" },
     { { 2045, 6, 1, 6, 15, 10, 0, 0 },   "\x17" "\x0d" "4506061610-01" },
     { { 2045, 6, 1, 6, 17, 10, 0, 0 },   "\x17" "\x0f" "4506061610+0100" },
     { { 2045, 6, 1, 6, 15, 10, 0, 0 },   "\x17" "\x0f" "4506061610-0100" },
    };
    /* An oddball case that succeeds in Windows, but doesn't seem correct
     { { 2145, 6, 1, 2, 11, 31, 0, 0 },   "\x18" "\x13" "21450606161000-9999" },
     */
    static const char *bogusTimes[] = {
     /* oddly, this succeeds on Windows, with year 2765
     "\x18" "\x0f" "21r50606161000Z",
      */
     "\x17" "\x08" "45060616",
     "\x18" "\x0f" "aaaaaaaaaaaaaaZ",
     "\x18" "\x04" "2145",
     "\x18" "\x08" "21450606",
    };
    DWORD i, size;
    FILETIME ft1 = { 0 }, ft2 = { 0 };
    BOOL ret;

    /* Check bogus length with non-NULL buffer */
    ret = SystemTimeToFileTime(&times[0].sysTime, &ft1);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    size = 1;
    ret = CryptDecodeObjectEx(dwEncoding, X509_CHOICE_OF_TIME,
     times[0].encodedTime, times[0].encodedTime[1] + 2, 0, NULL, &ft2, &size);
    ok(!ret && GetLastError() == ERROR_MORE_DATA,
     "Expected ERROR_MORE_DATA, got %ld\n", GetLastError());
    /* Normal tests */
    for (i = 0; i < sizeof(times) / sizeof(times[0]); i++)
    {
        testTimeDecoding(dwEncoding, X509_CHOICE_OF_TIME, &times[i]);
        testTimeDecoding(dwEncoding, PKCS_UTC_TIME, &times[i]);
        testTimeDecoding(dwEncoding, szOID_RSA_signingTime, &times[i]);
    }
    for (i = 0; i < sizeof(otherTimes) / sizeof(otherTimes[0]); i++)
    {
        testTimeDecoding(dwEncoding, X509_CHOICE_OF_TIME, &otherTimes[i]);
        testTimeDecoding(dwEncoding, PKCS_UTC_TIME, &otherTimes[i]);
        testTimeDecoding(dwEncoding, szOID_RSA_signingTime, &otherTimes[i]);
    }
    for (i = 0; i < sizeof(bogusTimes) / sizeof(bogusTimes[0]); i++)
    {
        size = sizeof(ft1);
        ret = CryptDecodeObjectEx(dwEncoding, X509_CHOICE_OF_TIME,
         bogusTimes[i], bogusTimes[i][1] + 2, 0, NULL, &ft1, &size);
        ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
         "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    }
}

struct EncodedName
{
    CERT_RDN_ATTR attr;
    BYTE *encoded;
};

static const char commonName[] = "Juan Lang";
static const char surName[] = "Lang";
static const char bogusIA5[] = "\x80";
static const char bogusPrintable[] = "~";
static const char bogusNumeric[] = "A";
static const struct EncodedName names[] = {
 { { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING,
   { sizeof(commonName), (BYTE *)commonName } },
 "\x30\x15\x31\x13\x30\x11\x06\x03\x55\x04\x03\x13\x0aJuan Lang" },
 { { szOID_COMMON_NAME, CERT_RDN_IA5_STRING,
   { sizeof(commonName), (BYTE *)commonName } },
 "\x30\x15\x31\x13\x30\x11\x06\x03\x55\x04\x03\x16\x0aJuan Lang" },
 { { szOID_SUR_NAME, CERT_RDN_IA5_STRING,
   { sizeof(surName), (BYTE *)surName } },
 "\x30\x10\x31\x0e\x30\x0c\x06\x03\x55\x04\x04\x16\x05Lang" },
 { { NULL, CERT_RDN_PRINTABLE_STRING,
   { sizeof(commonName), (BYTE *)commonName } },
 "\x30\x12\x31\x10\x30\x0e\x06\x00\x13\x0aJuan Lang" },
/* The following test isn't a very good one, because it doesn't encode any
 * Japanese characters.  I'm leaving it out for now.
 { { szOID_COMMON_NAME, CERT_RDN_T61_STRING,
   { sizeof(commonName), (BYTE *)commonName } },
 "\x30\x15\x31\x13\x30\x11\x06\x03\x55\x04\x03\x14\x0aJuan Lang" },
 */
 /* The following tests succeed under Windows, but really should fail,
  * they contain characters that are illegal for the encoding.  I'm
  * including them to justify my lazy encoding.
  */
 { { szOID_COMMON_NAME, CERT_RDN_IA5_STRING,
   { sizeof(bogusIA5), (BYTE *)bogusIA5 } },
 "\x30\x0d\x31\x0b\x30\x09\x06\x03\x55\x04\x03\x16\x02\x80" },
 { { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING,
   { sizeof(bogusPrintable), (BYTE *)bogusPrintable } },
 "\x30\x0d\x31\x0b\x30\x09\x06\x03\x55\x04\x03\x13\x02\x7e" },
 { { szOID_COMMON_NAME, CERT_RDN_NUMERIC_STRING,
   { sizeof(bogusNumeric), (BYTE *)bogusNumeric } },
 "\x30\x0d\x31\x0b\x30\x09\x06\x03\x55\x04\x03\x12\x02\x41" },
};

static const BYTE emptyName[] = { 0x30, 0 };
static const BYTE emptyRDNs[] = { 0x30, 0x02, 0x31, 0 };
static const BYTE twoRDNs[] = "\x30\x23\x31\x21\x30\x0c\x06\x03\x55\x04\x04"
 "\x13\x05\x4c\x61\x6e\x67\x00\x30\x11\x06\x03\x55\x04\x03"
 "\x13\x0a\x4a\x75\x61\x6e\x20\x4c\x61\x6e\x67";

static void test_encodeName(DWORD dwEncoding)
{
    CERT_RDN_ATTR attrs[2];
    CERT_RDN rdn;
    CERT_NAME_INFO info;
    BYTE *buf = NULL;
    DWORD size = 0, i;
    BOOL ret;

    /* Test with NULL pvStructInfo */
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, NULL,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* Test with empty CERT_NAME_INFO */
    info.cRDN = 0;
    info.rgRDN = NULL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, emptyName, sizeof(emptyName)),
         "Got unexpected encoding for empty name\n");
        LocalFree(buf);
    }
    /* Test with bogus CERT_RDN */
    info.cRDN = 1;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* Test with empty CERT_RDN */
    rdn.cRDNAttr = 0;
    rdn.rgRDNAttr = NULL;
    info.cRDN = 1;
    info.rgRDN = &rdn;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, emptyRDNs, sizeof(emptyRDNs)),
         "Got unexpected encoding for empty RDN array\n");
        LocalFree(buf);
    }
    /* Test with bogus attr array */
    rdn.cRDNAttr = 1;
    rdn.rgRDNAttr = NULL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* oddly, a bogus OID is accepted by Windows XP; not testing.
    attrs[0].pszObjId = "bogus";
    attrs[0].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[0].Value.cbData = sizeof(commonName);
    attrs[0].Value.pbData = (BYTE *)commonName;
    rdn.cRDNAttr = 1;
    rdn.rgRDNAttr = attrs;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret, "Expected failure, got success\n");
     */
    /* Check with two CERT_RDN_ATTRs.  Note DER encoding forces the order of
     * the encoded attributes to be swapped.
     */
    attrs[0].pszObjId = szOID_COMMON_NAME;
    attrs[0].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[0].Value.cbData = sizeof(commonName);
    attrs[0].Value.pbData = (BYTE *)commonName;
    attrs[1].pszObjId = szOID_SUR_NAME;
    attrs[1].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[1].Value.cbData = sizeof(surName);
    attrs[1].Value.pbData = (BYTE *)surName;
    rdn.cRDNAttr = 2;
    rdn.rgRDNAttr = attrs;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, twoRDNs, sizeof(twoRDNs)),
         "Got unexpected encoding for two RDN array\n");
        LocalFree(buf);
    }
    /* CERT_RDN_ANY_TYPE is too vague for X509_NAMEs, check the return */
    rdn.cRDNAttr = 1;
    attrs[0].dwValueType = CERT_RDN_ANY_TYPE;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        rdn.cRDNAttr = 1;
        rdn.rgRDNAttr = (CERT_RDN_ATTR *)&names[i].attr;
        ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(size == names[i].encoded[1] + 2, "Expected size %d, got %ld\n",
             names[i].encoded[1] + 2, size);
            ok(!memcmp(buf, names[i].encoded, names[i].encoded[1] + 2),
             "Got unexpected encoding\n");
            LocalFree(buf);
        }
    }
}

static void compareNames(const CERT_NAME_INFO *expected,
 const CERT_NAME_INFO *got)
{
    ok(got->cRDN == expected->cRDN, "Expected %ld RDNs, got %ld\n",
     expected->cRDN, got->cRDN);
    if (expected->cRDN)
    {
        ok(got->rgRDN[0].cRDNAttr == expected->rgRDN[0].cRDNAttr,
         "Expected %ld RDN attrs, got %ld\n", expected->rgRDN[0].cRDNAttr,
         got->rgRDN[0].cRDNAttr);
        if (expected->rgRDN[0].cRDNAttr)
        {
            if (expected->rgRDN[0].rgRDNAttr[0].pszObjId &&
             strlen(expected->rgRDN[0].rgRDNAttr[0].pszObjId))
            {
                ok(got->rgRDN[0].rgRDNAttr[0].pszObjId != NULL,
                 "Expected OID %s, got NULL\n",
                 expected->rgRDN[0].rgRDNAttr[0].pszObjId);
                if (got->rgRDN[0].rgRDNAttr[0].pszObjId)
                    ok(!strcmp(got->rgRDN[0].rgRDNAttr[0].pszObjId,
                     expected->rgRDN[0].rgRDNAttr[0].pszObjId),
                     "Got unexpected OID %s, expected %s\n",
                     got->rgRDN[0].rgRDNAttr[0].pszObjId,
                     expected->rgRDN[0].rgRDNAttr[0].pszObjId);
            }
            ok(got->rgRDN[0].rgRDNAttr[0].Value.cbData ==
             expected->rgRDN[0].rgRDNAttr[0].Value.cbData,
             "Unexpected data size, got %ld, expected %ld\n",
             got->rgRDN[0].rgRDNAttr[0].Value.cbData,
             expected->rgRDN[0].rgRDNAttr[0].Value.cbData);
            if (expected->rgRDN[0].rgRDNAttr[0].Value.pbData)
                ok(!memcmp(got->rgRDN[0].rgRDNAttr[0].Value.pbData,
                 expected->rgRDN[0].rgRDNAttr[0].Value.pbData,
                 expected->rgRDN[0].rgRDNAttr[0].Value.cbData),
                 "Unexpected value\n");
        }
    }
}

static void test_decodeName(DWORD dwEncoding)
{
    int i;
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    BOOL ret;
    CERT_RDN rdn;
    CERT_NAME_INFO info = { 1, &rdn };

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        /* When the output buffer is NULL, this always succeeds */
        SetLastError(0xdeadbeef);
        ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, names[i].encoded,
         names[i].encoded[1] + 2, 0, NULL, NULL, &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %08lx\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, names[i].encoded,
         names[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
         (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        rdn.cRDNAttr = 1;
        rdn.rgRDNAttr = (CERT_RDN_ATTR *)&names[i].attr;
        if (buf)
        {
            compareNames((CERT_NAME_INFO *)buf, &info);
            LocalFree(buf);
        }
    }
    /* test empty name */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, emptyName,
     emptyName[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    /* Interestingly, in Windows, if cRDN is 0, rgRGN may not be NULL.  My
     * decoder works the same way, so only test the count.
     */
    if (buf)
    {
        ok(bufSize == sizeof(CERT_NAME_INFO),
         "Expected bufSize %d, got %ld\n", sizeof(CERT_NAME_INFO), bufSize);
        ok(((CERT_NAME_INFO *)buf)->cRDN == 0,
         "Expected 0 RDNs in empty info, got %ld\n",
         ((CERT_NAME_INFO *)buf)->cRDN);
        LocalFree(buf);
    }
    /* test empty RDN */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, emptyRDNs,
     emptyRDNs[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_NAME_INFO *info = (CERT_NAME_INFO *)buf;

        ok(bufSize == sizeof(CERT_NAME_INFO) + sizeof(CERT_RDN) &&
         info->cRDN == 1 && info->rgRDN && info->rgRDN[0].cRDNAttr == 0,
         "Got unexpected value for empty RDN\n");
        LocalFree(buf);
    }
    /* test two RDN attrs */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, twoRDNs,
     twoRDNs[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_RDN_ATTR attrs[] = {
         { szOID_SUR_NAME, CERT_RDN_PRINTABLE_STRING, { sizeof(surName),
          (BYTE *)surName } },
         { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING, { sizeof(commonName),
          (BYTE *)commonName } },
        };

        rdn.cRDNAttr = sizeof(attrs) / sizeof(attrs[0]);
        rdn.rgRDNAttr = attrs;
        compareNames((CERT_NAME_INFO *)buf, &info);
        LocalFree(buf);
    }
}

struct encodedOctets
{
    BYTE *val;
    BYTE *encoded;
};

static const struct encodedOctets octets[] = {
    { "hi", "\x04\x02hi" },
    { "somelong\xffstring", "\x04\x0fsomelong\xffstring" },
    { "", "\x04\x00" },
};

static void test_encodeOctets(DWORD dwEncoding)
{
    CRYPT_DATA_BLOB blob;
    DWORD i;

    for (i = 0; i < sizeof(octets) / sizeof(octets[0]); i++)
    {
        BYTE *buf = NULL;
        BOOL ret;
        DWORD bufSize = 0;

        blob.cbData = strlen(octets[i].val);
        blob.pbData = (BYTE *)octets[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_OCTET_STRING, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 4,
             "Got unexpected type %d for octet string (expected 4)\n", buf[0]);
            ok(buf[1] == octets[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], octets[i].encoded[1]);
            ok(!memcmp(buf + 1, octets[i].encoded + 1,
             octets[i].encoded[1] + 1), "Got unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeOctets(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(octets) / sizeof(octets[0]); i++)
    {
        BYTE *buf = NULL;
        BOOL ret;
        DWORD bufSize = 0;

        ret = CryptDecodeObjectEx(dwEncoding, X509_OCTET_STRING,
         (BYTE *)octets[i].encoded, octets[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_DATA_BLOB) + octets[i].encoded[1],
         "Expected size >= %d, got %ld\n",
         sizeof(CRYPT_DATA_BLOB) + octets[i].encoded[1], bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_DATA_BLOB *blob = (CRYPT_DATA_BLOB *)buf;

            if (blob->cbData)
                ok(!memcmp(blob->pbData, octets[i].val, blob->cbData),
                 "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static const BYTE bytesToEncode[] = { 0xff, 0xff };

struct encodedBits
{
    DWORD cUnusedBits;
    BYTE *encoded;
    DWORD cbDecoded;
    BYTE *decoded;
};

static const struct encodedBits bits[] = {
    /* normal test case */
    { 1, "\x03\x03\x01\xff\xfe", 2, "\xff\xfe" },
    /* strange test case, showing cUnusedBits >= 8 is allowed */
    { 9, "\x03\x02\x01\xfe", 1, "\xfe" },
    /* even stranger test case, showing cUnusedBits > cbData * 8 is allowed */
    { 17, "\x03\x01\x00", 0, NULL },
};

static void test_encodeBits(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
        CRYPT_BIT_BLOB blob;
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        blob.cbData = sizeof(bytesToEncode);
        blob.pbData = (BYTE *)bytesToEncode;
        blob.cUnusedBits = bits[i].cUnusedBits;
        ret = CryptEncodeObjectEx(dwEncoding, X509_BITS, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == bits[i].encoded[1] + 2,
             "Got unexpected size %ld, expected %d\n", bufSize,
             bits[i].encoded[1] + 2);
            ok(!memcmp(buf, bits[i].encoded, bits[i].encoded[1] + 2),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeBits(DWORD dwEncoding)
{
    static const BYTE ber[] = "\x03\x02\x01\xff";
    static const BYTE berDecoded = 0xfe;
    DWORD i;
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    /* normal cases */
    for (i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_BITS, bits[i].encoded,
         bits[i].encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
         &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            CRYPT_BIT_BLOB *blob;

            ok(bufSize >= sizeof(CRYPT_BIT_BLOB) + bits[i].cbDecoded,
             "Got unexpected size %ld, expected >= %ld\n", bufSize,
             sizeof(CRYPT_BIT_BLOB) + bits[i].cbDecoded);
            blob = (CRYPT_BIT_BLOB *)buf;
            ok(blob->cbData == bits[i].cbDecoded,
             "Got unexpected length %ld, expected %ld\n", blob->cbData,
             bits[i].cbDecoded);
            if (blob->cbData && bits[i].cbDecoded)
                ok(!memcmp(blob->pbData, bits[i].decoded, bits[i].cbDecoded),
                 "Unexpected value\n");
            LocalFree(buf);
        }
    }
    /* special case: check that something that's valid in BER but not in DER
     * decodes successfully
     */
    ret = CryptDecodeObjectEx(dwEncoding, X509_BITS, ber, ber[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRYPT_BIT_BLOB *blob;

        ok(bufSize >= sizeof(CRYPT_BIT_BLOB) + sizeof(berDecoded),
         "Got unexpected size %ld, expected >= %d\n", bufSize,
         sizeof(CRYPT_BIT_BLOB) + berDecoded);
        blob = (CRYPT_BIT_BLOB *)buf;
        ok(blob->cbData == sizeof(berDecoded),
         "Got unexpected length %ld, expected %d\n", blob->cbData,
         sizeof(berDecoded));
        if (blob->cbData)
            ok(*blob->pbData == berDecoded, "Unexpected value\n");
        LocalFree(buf);
    }
}

static void test_registerOIDFunction(void)
{
    static const WCHAR bogusDll[] = { 'b','o','g','u','s','.','d','l','l',0 };
    BOOL ret;

    /* oddly, this succeeds under WinXP; the function name key is merely
     * omitted.  This may be a side effect of the registry code, I don't know.
     * I don't check it because I doubt anyone would depend on it.
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, NULL,
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
     */
    /* On windows XP, GetLastError is incorrectly being set with an HRESULT,
     * HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)
     */
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "foo", NULL, bogusDll,
     NULL);
    ok(!ret && (GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() ==
     HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)),
     "Expected ERROR_INVALID_PARAMETER: %ld\n", GetLastError());
    /* This has no effect, but "succeeds" on XP */
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "foo",
     "1.2.3.4.5.6.7.8.9.10", NULL, NULL);
    ok(ret, "Expected pseudo-success, got %ld\n", GetLastError());
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(X509_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "bogus",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(X509_ASN_ENCODING, "bogus",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
    /* This has no effect */
    ret = CryptRegisterOIDFunction(PKCS_7_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    /* Check with bogus encoding type: */
    ret = CryptRegisterOIDFunction(0, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    /* This is written with value 3 verbatim.  Thus, the encoding type isn't
     * (for now) treated as a mask.
     */
    ret = CryptRegisterOIDFunction(3, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(3, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
}

START_TEST(encode)
{
    static const DWORD encodings[] = { X509_ASN_ENCODING, PKCS_7_ASN_ENCODING,
     X509_ASN_ENCODING | PKCS_7_ASN_ENCODING };
    DWORD i;

    for (i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
    {
        test_encodeInt(encodings[i]);
        test_decodeInt(encodings[i]);
        test_encodeEnumerated(encodings[i]);
        test_decodeEnumerated(encodings[i]);
        test_encodeFiletime(encodings[i]);
        test_decodeFiletime(encodings[i]);
        test_encodeName(encodings[i]);
        test_decodeName(encodings[i]);
        test_encodeOctets(encodings[i]);
        test_decodeOctets(encodings[i]);
        test_encodeBits(encodings[i]);
        test_decodeBits(encodings[i]);
    }
    test_registerOIDFunction();
}
