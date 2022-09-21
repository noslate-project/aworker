#include "binding/core/text.h"
#include <unicode/uidna.h>
#include <unicode/utypes.h>
#include <cmath>
#include <codecvt>
#include <locale>
#include <string>

namespace aworker {
using v8::Isolate;
using v8::Local;
using v8::String;

int32_t domainToASCII(std::string* buf,
                      const char* input,
                      size_t length,
                      bool be_strict) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t options =                   // CheckHyphens = false; handled later
      UIDNA_CHECK_BIDI |               // CheckBidi = true
      UIDNA_CHECK_CONTEXTJ |           // CheckJoiners = true
      UIDNA_NONTRANSITIONAL_TO_ASCII;  // Nontransitional_Processing
  if (be_strict) {
    options |= UIDNA_USE_STD3_RULES;  // UseSTD3ASCIIRules = beStrict
                                      // VerifyDnsLength = beStrict;
                                      //   handled later
  }

  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status)) return -1;

  UIDNAInfo info = UIDNA_INFO_INITIALIZER;

  char* temp = new char[3];
  int32_t len =
      uidna_nameToASCII_UTF8(uidna, input, length, temp, 3, &info, &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    delete[] temp;
    temp = new char[len];

    len =
        uidna_nameToASCII_UTF8(uidna, input, length, temp, len, &info, &status);
  }

  // In UTS #46 which specifies ToASCII, certain error conditions are
  // configurable through options, and the WHATWG URL Standard promptly elects
  // to disable some of them to accommodate for real-world use cases.
  // Unfortunately, ICU4C's IDNA module does not support disabling some of
  // these options through `options` above, and thus continues throwing
  // unnecessary errors. To counter this situation, we just filter out the
  // errors that may have happened afterwards, before deciding whether to
  // return an error from this function.

  // CheckHyphens = false
  // (Specified in the current UTS #46 draft rev. 18.)
  // Refs:
  // - https://github.com/whatwg/url/issues/53
  // - https://github.com/whatwg/url/pull/309
  // - http://www.unicode.org/review/pri317/
  // - http://www.unicode.org/reports/tr46/tr46-18.html
  // - https://www.icann.org/news/announcement-2000-01-07-en
  info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
  info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
  info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

  // VerifyDnsLength = beStrict
  if (!be_strict) {
    // filter error codes.
    info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
    info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
    info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;
  }

  if (U_FAILURE(status) || info.errors != 0) {
    len = -1;
  } else {
    *buf = std::string(temp, len);
  }

  delete[] temp;
  uidna_close(uidna);
  return len;
}

}  // namespace aworker
