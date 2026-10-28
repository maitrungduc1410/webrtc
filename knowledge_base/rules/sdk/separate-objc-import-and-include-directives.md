---
name: webrtc/sdk/separate-objc-import-and-include-directives
description: 'In Objective-C and Objective-C++ files, use #import for Objective-C headers and #include for C/C++ headers, grouping all #import blocks before #include blocks'
rubric: readability
severity: should_fix
---

**Rule.** In Objective-C and Objective-C++ files (.h, .m, .mm), use `#import`
for Objective-C headers and Apple platform system frameworks (e.g., sdk/objc/*,
NSString+*.h, `<Foundation/Foundation.h>`), and use `#include` for pure C and
C++ headers (such as api/*, rtc_base/*, and standard library headers). Group all
`#import` directives together before `#include` directives in distinct blocks
separated by blank lines rather than interleaving them or using `#include` for
Objective-C headers.

**Why.** In Objective-C development, `#import` guarantees header files and
system frameworks are not included multiple times even without header guards,
conforming to Chromium and Apple Objective-C style conventions. Conversely,
using `#import` for C/C++ headers breaks downstream C++ toolchains and modular
builds that do not support import semantics. Grouping `#import` blocks before
`#include` blocks maintains clear module boundaries and consistent dependency
ordering across SDK files.

## Examples

Bad:

```cpp
// Bad: Using #include for Obj-C headers, #import for C++ headers, and interleaving them.
#include <Foundation/Foundation.h>
#include "helpers/NSString+StdString.h"
#include "sdk/objc/base/RTCMacros.h"
#import <memory>
#import "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
```

Good:

```cpp
// Good: #import for Objective-C headers and system frameworks, grouped first.
#import <Foundation/Foundation.h>
#import "helpers/NSString+StdString.h"
#import "sdk/objc/base/RTCMacros.h"

// Good: #include for C and C++ headers, grouped after #import blocks.
#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
```
