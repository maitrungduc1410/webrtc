# This file contains dependencies for WebRTC.

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'android_ndk_version',
  'generate_location_tags',
]

vars = {
  # Keep the Chromium default for these gn args.
  'generate_location_tags': True,
  'android_ndk_version': Str('2@30.0.16248370'),

  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'chromium_revision': '2fa7a6bc28c175c3f8209f659c299ec489fa73ee',

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  'chromium_git': 'https://chromium.googlesource.com',

  # ResultDB version
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',
  'fuchsia_version': 'version:34.20261123.5.1',
  # By default, download the fuchsia images from the fuchsia GCS bucket.
  'fuchsia_images_bucket': 'fuchsia',
  'checkout_fuchsia': False,
  # Since the images are hundreds of MB, default to only downloading the image
  # most commonly useful for developers. Bots and developers that need to use
  # other images can override this with additional images.
  'checkout_fuchsia_boot_images': "terminal.x64",
  'checkout_fuchsia_product_bundles': '"{checkout_fuchsia_boot_images}" != ""',

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,
  # RBE instance to use for running remote builds
  'rbe_instance': 'projects/rbe-webrtc-developer/instances/default_instance',
  # siso CIPD package version.
  'siso_version': 'git_revision:3025be9c477069426f08b8235e9545d5b20afd0f',

  # CPython 3 CIPD package version for Siso hermetic toolchain.
  'cpython3_version': 'version:3@3.11.9.chromium.38',

  # Android aapt2 CIPD package version.
  'aapt2_version': 'version:2@9.6.0-alpha05-16953823',

  # ninja CIPD package.
  'ninja_package': 'infra/3pp/tools/ninja/',

  # ninja CIPD package version
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

  # condition to allowlist deps for non-git-source processing.
  'non_git_source': 'True',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,
}

deps = {
  'src/build':
    'https://chromium.googlesource.com/chromium/src/build@f71636db2fdbb0edce7ffcf76877ee6097de72f1',
  'src/buildtools':
    'https://chromium.googlesource.com/chromium/src/buildtools@9411a4f3bf18c99b30c1a817b5bc6280a6530a5c',
  # Gradle 6.6.1. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': 'https://chromium.googlesource.com/external/github.com/gradle/gradle.git@f2d1fb54a951d8b11d25748e4711bec8d128d7e3',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': 'https://chromium.googlesource.com/chromium/src/ios@2c36eea1f1c0e3a93831b990b249f0c4fb6dd280',
    'condition': 'checkout_ios',
  },
  'src/testing':
    'https://chromium.googlesource.com/chromium/src/testing@1ffb904b33e5b876f4b2355caf3a4a937d898512',
  'src/third_party':
    'https://chromium.googlesource.com/chromium/src/third_party@79e19b9a42480c4e36abe2cf798a051694409e72',

  'src/buildtools/third_party/mold/cipd': {
      'packages': [
          {
              'package': 'chromium/buildtools/third_party/mold/mold',
              'version': 'w9QqZ-oP3GovA-zjOawYzb5yca-t47GbT-ejPa0BQmYC',
          },
      ],
      'condition': 'host_os == "linux" and non_git_source',
      'dep_type': 'cipd',
  },
  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:e04c7ff10dc0955050fbdbae6b46d856663d7d1f',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:e04c7ff10dc0955050fbdbae6b46d856663d7d1f',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:e04c7ff10dc0955050fbdbae6b46d856663d7d1f',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win',
  },

  'src/third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'condition': 'not llvm_force_head_revision',
    'objects': [
      {
        'object_name': 'Linux_x64/clang-android-runtime-library-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '64727394243fcab5a1c2765da26d8f1e8ebbd9fe7f35d0cab3b3a0da9fde7d4a',
        'size_bytes': 6181828,
        'generation': 1795478778225891,
        'condition': 'checkout_android and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-linux-runtime-library-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '0df7ba9ee1c1d81eaa49824a3d3ac90a288e9fa9e035f770e7d58e2cebf1a149',
        'size_bytes': 3153216,
        'generation': 1795478783056858,
        'condition': 'checkout_linux and not host_os == "linux"',
      },
      {
        'object_name': 'Linux_x64/clang-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'a3fbdb1855e705711d5fb5892f88cc69a8f0cd9a4ee04a0850c89b7d2b8b179f',
        'size_bytes': 59066320,
        'generation': 1795478775605938,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '36775600bb383b755ae465984bdafda01043dd7ac61083cbab902a6ec27b024a',
        'size_bytes': 15220244,
        'generation': 1795478775715724,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'e0de5f4f8845a131af2061eff24ba4563255914220145d05ab4a7b924896b851',
        'size_bytes': 15583152,
        'generation': 1795478775875977,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '86732aeed8b70985b77f614213f88685ebb569e5da35d524442cf15279c82385',
        'size_bytes': 2435668,
        'generation': 1795478776072148,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '192e516a17b549336da0c1991feebb5945164c7d9e35003c71de47a496ae3a1a',
        'size_bytes': 6024552,
        'generation': 1795478775918517,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'a5fa83d990ea74c1f90c44ce063deb53fa90b39e4eeab0c9b1170dddcab33157',
        'size_bytes': 55567376,
        'generation': 1795478784445219,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'c8c40924e87d77bc81e4b7fe914e2e5ffcc0ba26bb0e5cef9dbfb7f1c9b9d365',
        'size_bytes': 1286412,
        'generation': 1795478791497407,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '9cd2a83dbccb2cab4fe50a5ee282db91ccf5812321d73a69c66285ec05d6b606',
        'size_bytes': 14481732,
        'generation': 1795478784536728,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'adf9d309e14fbf41621acd303cbdb1eb09e13063ece7161c6a643ea85224da1b',
        'size_bytes': 15583700,
        'generation': 1795478784642991,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '2efdec6e0ecf9805b99dce21a2dbb1c0973ad542c456f7c22f76df7a27dbd84b',
        'size_bytes': 2315360,
        'generation': 1795478784842581,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'b59bca56abaa08663e82781a248b1402da20c6dbb1abe4241c7d806745202724',
        'size_bytes': 5597588,
        'generation': 1795478784732320,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'fa0844933f1afeca7f35866e865beed5e7806d5eec15db941ef8e4c5141f2f24',
        'size_bytes': 45625592,
        'generation': 1795478792867551,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '60742ca1f3bd526ddead079039808f2c872b8f38713aba895c964eff5e8b7ba5',
        'size_bytes': 12359740,
        'generation': 1795478792976527,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': 'b858d19a2fe2bb80a9fea92780d258ad9101511cac7eec82816cafdbfdf1b099',
        'size_bytes': 12333420,
        'generation': 1795478793090638,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '28d07e2a7e0594678e01febf05d0643bb8f6e82b6a460129837527236be40063',
        'size_bytes': 1902196,
        'generation': 1795478793241857,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '2e6f70ac5a2f05743f45499b36887a86933aed585c5a7626879efe0d83981487',
        'size_bytes': 5282828,
        'generation': 1795478793129620,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '621d50c0f30b036c6a63c216e3048b8551983b103344523448143da927e3c3d2',
        'size_bytes': 53390252,
        'generation': 1795478801750733,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '7f06cf789c7ed97f9e0634559392844652cb8a1d9335922f7c3bc3b8cd931410',
        'size_bytes': 15535600,
        'generation': 1795478801910632,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '5aa388fc2eb08fc26272f4d20f766de868e407181f940dee0d1fbb22c5f5f3d4',
        'size_bytes': 2714456,
        'generation': 1795478808882265,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '3596d131f50d7d5dce14898d6eee747aaab042438d237a35d72fcdc554641dfd',
        'size_bytes': 15969244,
        'generation': 1795478802031503,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '3261cff4039624b9cd51ad7d9b4fd42d129147c840a61edb4684df7bcdb3c10f',
        'size_bytes': 2617712,
        'generation': 1795478802200524,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-24-init-17044-gc3223914-68.tar.xz',
        'sha256sum': '2ec95a789c636d0a0fdbcb4a7fc90e7cbc0e45eda840d57a2407e20e0389d429',
        'size_bytes': 6132588,
        'generation': 1795478802113233,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  # Update prebuilt Rust toolchain.
  'src/third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '73e395668def3e0a488f5edffd3ccaeb86a99f0476ed06c04930c6895094b212',
        'size_bytes': 287866428,
        'generation': 1794385348294495,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '0888daf1c1215ac4556028d7e6df7d5fa0c57336bff64c570a57f93fef0da565',
        'size_bytes': 275787508,
        'generation': 1794385352157176,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '64a506650c18e57eeae11d493c41faf3a712885b7b65d993f4c76eac7f0498b7',
        'size_bytes': 259550672,
        'generation': 1794385356196372,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': 'c3c1f2e465ed50a67e2ba5c5579c3df46e2fb445ccb73d063869087972d84e1c',
        'size_bytes': 427637256,
        'generation': 1794385360031394,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'src/third_party/clang-format/script':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@65b9a76faeba5f10c1f331ad729b3bae89defbbb',
  'src/third_party/compiler-rt/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt.git@682f738bc1e798f466a1dda835101f28b8dfe797',
  'src/third_party/libc++/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git@2c7b4048084397454ff423895c23c40a86dbf214',
  'src/third_party/libc++abi/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxxabi.git@8e42446dd61148a66220123297964bd3856e7cf9',
  'src/third_party/llvm-libc/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libc.git@f95dd12ba6da85518ff2eaa384bdff3716612f1a',
  'src/third_party/libunwind/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libunwind.git@9e44ea2ff4f53a60c2a3d2b6f511d7bab9ab12f0',

  'src/third_party/test_fonts/test_fonts': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-fonts',
      'objects': [
          {
              'object_name': '9c07d19d9c5ee1ff94f717e6fb17e0c8c354e6f9',
              'sha256sum': 'f0e9628f9e43e3f3476cde06a1849058de460e0e037b7449ce0d42b9a73c37d5',
              'size_bytes': 33413117,
              'generation': 1775663926405386,
          },
      ],
  },

  'src/third_party/ninja': {
    'packages': [
      {
        'package': Var('ninja_package') + '${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/siso/cipd': {
    'packages': [
      {
        'package': 'build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'condition': 'non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_system_sdk/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': 'QAcOcO9AlzRvhHOaFCxeQAcdO3qiFPwxUHXUIlSbw3kC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/tools/resultdb': {
    'packages': [
      {
        'package': 'infra/tools/result_adapter/${{platform}}',
        'version': Var('result_adapter_revision'),
      },
    ],
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/aapt2/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2/${{platform}}',
              'version': Var('aapt2_version'),
          },
      ],
      'condition': 'checkout_android and non_git_source and '
                   '((host_os == "linux" and host_cpu == "x64") or '
                   '(host_os == "mac" and host_cpu == "arm64"))',
      'dep_type': 'cipd',
  },

  # Resources are compiled on Linux workers under remote execution, so
  # non-Linux hosts also need the Linux aapt2.
  'src/third_party/android_build_tools/aapt2/linux-amd64': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2/linux-amd64',
              'version': Var('aapt2_version'),
          },
      ],
      'condition': 'checkout_android and non_git_source and host_os != "linux"',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bundletool/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': '7Vo6ZzIxIaC51ATTBlo_KUkgxJCmmGmXAijlVkXUTpAC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone',
               'version': 'SWWYuvjTXZ5wEv1X5nQ9eCFNyKKCu1ePkuEeQUaW7ScC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone_javac/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone_javac',
               'version': '7EcHxlEXEaLRWEyHIAxf0ouPjkmN1Od6jkutuo0sfBIC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/lint/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/lint',
               'version': 'EMvaA_bqOEaSua9qeImqZ5KdniW5lXml-v0yjDoiIJ0C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },



  'src/third_party/aosp_dalvik/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/aosp_dalvik/linux-amd64',
              'version': 'version:2@13.0.0_r24.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git@2e33b24cdb5b021c07990dc6d0191ae4a1480cfb',
  'src/third_party/breakpad/breakpad':
    'https://chromium.googlesource.com/breakpad/breakpad.git@f9f68ccdb33a188e879b5fc0a58d23e84c9b9ae9',
  'src/third_party/catapult':
    'https://chromium.googlesource.com/catapult.git@c293f274909cbd4c7e34b2ac765bc1c880addca9',
  'src/third_party/ced/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/compact_enc_det.git@d127078cedef9c6642cbe592dacdd2292b50bb19',
  },
  'src/third_party/colorama/src':
    'https://chromium.googlesource.com/external/colorama.git@3de9f013df4b470069d03d250224062e8cf15c49',
  'src/third_party/cpu_features/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/cpu_features.git@81d13c49649f0714dd41fb56bb246398b6584085',
    'condition': 'checkout_android',
  },
  'src/third_party/crc32c/src':
    'https://chromium.googlesource.com/external/github.com/google/crc32c.git@2bbb3be42e20a0e6c0f7b39dc07dc863d9ffbc07',
  'src/third_party/depot_tools':
    'https://chromium.googlesource.com/chromium/tools/depot_tools.git@ee90e3f95824aa10ed7a912fe08e16e45eb62004',
  'src/third_party/ffmpeg':
    'https://chromium.googlesource.com/chromium/third_party/ffmpeg.git@878e4c23c639a9ad862bf57852fbc6bb34f40c50',
  'src/third_party/flatbuffers/src':
    'https://chromium.googlesource.com/external/github.com/google/flatbuffers.git@a86afae9399bbe631d1ea0783f8816e780e236cc',
  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': 'https://chromium.googlesource.com/external/fontconfig.git@bb21be622280981392c0335fc22202fbba46d247',
      'condition': 'checkout_linux',
  },
  'src/third_party/freetype/src':
    'https://chromium.googlesource.com/chromium/src/third_party/freetype2.git@6065d6a40effdef3d5756642378c2b683dfc51d5',
  'src/third_party/harfbuzz/src':
    'https://chromium.googlesource.com/external/github.com/harfbuzz/harfbuzz.git@c8f3caa93552614c7f2dac71d506671fbe07dacf',
  'src/third_party/google_benchmark/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/benchmark.git@a6b4a9f00b36c19af9997d68a1cf6e0b5b3d032c',
  },
  'src/third_party/libpfm4/src':
    Var('chromium_git') + '/external/git.code.sf.net/p/perfmon2/libpfm4.git' + '@' + '186a658c3e1e49b0ee3da414a7db8ba971a00cb5',
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/gtest-parallel':
    'https://chromium.googlesource.com/external/github.com/google/gtest-parallel@cd488bdedc1d2cffb98201a17afc1b298b0b90f1',
  'src/third_party/google-truth/src': {
      'url': 'https://chromium.googlesource.com/external/github.com/google/truth.git@c343af57257140721159a81198733520a479b297',
      'condition': 'checkout_android',
  },
  'src/third_party/googletest/src':
    'https://chromium.googlesource.com/external/github.com/google/googletest.git@a46a6012a133c7c0929fdae55b321c2fe65f3c5f',

  # Always download Linux x64 package regardless of host OS for RBE workers.
  'src/third_party/cpython3/linux-amd64': {
      'packages': [
        {
          'package': 'infra/3pp/tools/cpython3/linux-amd64',
          'version': Var('cpython3_version'),
        },
      ],
      'condition': 'non_git_source',
      'dep_type': 'cipd',
  },

  # Host platform package. ${platform} folder is not used as in .gn the variable
  # is not initialized yet by the time Python is required.
  'src/third_party/cpython3/host': {
      'packages': [
        {
          'package': 'infra/3pp/tools/cpython3/${{platform}}',
          'version': Var('cpython3_version'),
        },
      ],
      'condition': 'non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/icu': {
    'url': 'https://chromium.googlesource.com/chromium/deps/icu.git@d3c612f4414600bd06a68db89944df130b5bc248',
  },
  'src/third_party/jdk/current': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/${{platform}}',
              'version': 'version:2@jdk-25.0.5.1+2.4b2a0125e8.cr0',
          },
      ],
      # Needed on Linux for use on chromium_presubmit (for checkstyle).
      'condition': '(checkout_android or checkout_linux) and non_git_source',
      'dep_type': 'cipd',
  },
  # Deprecated - only use for tools which are broken real JDK.
  # Not used by WebRTC. Added for compatibility with Chromium.
  'src/third_party/jdk11': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              # Do not update this hash - any newer hash will point to JDK17+.
              'version': 'egbcSHbmF1XZQbKxp_PQiGLFWlQK65krTGqQE-Bj4j8C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/jsoncpp/src':
    'https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp.git@93acc7b155048f1a66c43235cc205519f1e7a6ea',
  'src/third_party/junit/src': {
    'url': 'https://chromium.googlesource.com/external/junit.git@890f3c972647de378f25e7271d8fbbd9d3456b79',
    'condition': 'checkout_android',
  },
  'src/third_party/kotlinc/current': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlinc',
              'version': 'HySY0uWxnnFPAy76yRKsyhRENtUIP9OMqCloO1FQErEC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/libFuzzer/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git@c7ea90fb61105752fa78e257e77a7c5d62fff7d3',
  'src/third_party/fuzztest/src':
    'https://chromium.googlesource.com/external/github.com/google/fuzztest.git@b7ef234c594beeb90ef6089fa30d6b3c0c65e82b',
  'src/third_party/libprotobuf-mutator/src':
    Var('chromium_git') + '/external/github.com/google/libprotobuf-mutator.git@a0448e5055e4740774e00d496f016d9fcd572200',
  'src/third_party/libjpeg_turbo':
    'https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git@640f254ad0fa03f6b1f29f89b7dd9366f2f6e533',
  'src/third_party/libsrtp':
    'https://chromium.googlesource.com/chromium/deps/libsrtp.git@cd5d177bf1fde755ddb4c7f0d9ff7693f8b49e5e',
  'src/third_party/dav1d/libdav1d':
    'https://chromium.googlesource.com/external/github.com/videolan/dav1d.git@918e7a67461da56f9c7b5c20842ea1d81b613945',
  'src/third_party/libaom/source/libaom':
    'https://aomedia.googlesource.com/aom.git@93f371ba7baebeb7086aed440996274e6dc020a6',
  'src/third_party/libgav1/src':
    Var('chromium_git') + '/codecs/libgav1.git' + '@' + '9f78a5b81409f4880ec0c96cd422851aab9d6d74',
  'src/third_party/perfetto':
    Var('chromium_git') + '/external/github.com/google/perfetto.git' + '@' + 'e6a2946f0b3e0d604db6eeec82328e2b293e32e6',
  'src/third_party/protobuf-javascript/src':
    Var('chromium_git') + '/external/github.com/protocolbuffers/protobuf-javascript' + '@' + 'eb59fef711c3d090d54b988b99b7c067d6b7e3a8',
  'src/third_party/libvpx/source/libvpx':
    'https://chromium.googlesource.com/webm/libvpx.git@0c7afa13cc76906a0895eb8661703373561b37c7',
  'src/third_party/libyuv':
    'https://chromium.googlesource.com/libyuv/libyuv.git@1e2e3a35993609da1d89ac170b907719d555f4b3',
  'src/third_party/lss': {
    'url': 'https://chromium.googlesource.com/linux-syscall-support.git@29164a80da4d41134950d76d55199ea33fbb9613',
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': 'https://chromium.googlesource.com/external/mockito/mockito.git@04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' + '@' + 'fd1b8502e69c84a128d2b49259472ab93f7ea8ec',
    'condition': 'checkout_instrumented_libraries',
  },

  # Used by boringssl.
  'src/third_party/nasm': {
      'url': 'https://chromium.googlesource.com/chromium/deps/nasm.git@54b1dcf7995006337c54f6862fd6f77a01e23e4e'
  },

  'src/third_party/openh264/src':
    'https://chromium.googlesource.com/external/github.com/cisco/openh264@78783f195ce116ecbaea83f7fac0ea8a355518b5',

  'src/third_party/re2/src':
    'https://chromium.googlesource.com/external/github.com/google/re2.git@2da0056814cf180480a19f5cf811e7e1c054bf6d',

  'src/third_party/sframe/src':
    Var('chromium_git') + '/external/github.com/cisco/sframe' + '@' + '6a37f061f4eda0c6594c91ecf99b5ff498c8c124',

  'src/third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'Rq20KakoysMf7EM9-XMfjX7bqfwxNbcbPb8xx09gvrwC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_build_tools/signflinger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/signflinger',
               'version': 'sR382zqUwar3urYhP_Zf4n9TGzyuJQRGJznAtGCSGOUC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },
  'src/third_party/requests/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/kennethreitz/requests.git@c2b307dbefe21177af03f9feb37181a89a799fcc',
    'condition': 'checkout_android',
  },
  'src/tools':
    'https://chromium.googlesource.com/chromium/src/tools@79c31f1f3d59d466be1feb8b3f6ff4b4d0d62f22',

  'src/third_party/espresso': {
      'packages': [
          {
              'package': 'chromium/third_party/espresso',
              'version': '5LoBT0j383h_4dXbnap7gnNQMtMjpbMJD1JaGIYNj-IC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/hamcrest/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': 'dBioOAmFJjqAr_DY7dipbXdVfAxUQwjOBNibMPtX8lQC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_toolchain/ndk': {
    'packages': [
      {
        'package': 'chromium/third_party/android_toolchain/android_toolchain/${{platform}}',
        'version': 'version:2@30.0.16248370',
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },

  'src/third_party/androidx/cipd': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': 'TVT3x6-8ZfD4js9vUuKrjuYgYdTCI_D6Z0I395xETIAC',
      },
    ],
    'condition': 'checkout_android and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bazel_tools/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/bazel_tools/${{platform}}',
              'version': 'version:2@7.4.1',
          },
      ],
      'condition': 'checkout_android and non_git_source and '
                   '((host_os == "linux" and host_cpu == "x64") or '
                   '(host_os == "mac" and host_cpu == "arm64"))',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'hRhDlqIUkkCI_dkXXn7IZaK4Y9zI7ryZ7KAuRQ6sEkgC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/37.0.0/${{os}}',
              'version': 'version_37.0.0',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': 'qTD9QdBlBf3dyHsN1lJ0RH6AhHxR42Hmg2Ih-Vj4zIEC'
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-37.2',
              'version': 'AAswZ5rZBFDhMaH4QbKPmVtP4vnjiNUW_zPhdWvNe6wC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools/${{os}}',
              'version': 'version:23.0.0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/icu4j/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': '8dV7WRVX0tTaNNqkLEnCA_dMofr2MJXFK400E7gOFygC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/robolectric/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric/${{platform}}',
              'version': 'version:2@android-all-17-robolectric-16653203-5af0e5518e',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/sqlite4java/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': 'LofjKH9dgXIAJhRYCPQlMFywSwxYimrfDeBmaHc-Z5EC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/tflite/src':
    Var('chromium_git') + '/external/github.com/tensorflow/tensorflow.git' + '@' + '78c63d8e372d80c6d5688aac438468c075832f1e',

  'src/third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'SE6gAVnO8FA5pZPiFMFNNMB_84TrBa72xOPilnA4VbEC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/zstd/src': {
    'url': Var('chromium_git') + '/external/github.com/facebook/zstd.git' + '@' + '10da6ba6de05e29169261fa4b68eb99239f770dd',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/cas/${{platform}}',
          'version': 'git_revision:e91c778d73cde10d6d995c3cb003616b9d57fc78',
        },
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': 'git_revision:e91c778d73cde10d6d995c3cb003616b9d57fc78',
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': 'git_revision:e91c778d73cde10d6d995c3cb003616b9d57fc78',
        }
      ],
      'dep_type': 'cipd',
  },

  'src/third_party/pipewire/src': {
    'url': Var('chromium_git') + '/external/gitlab.freedesktop.org/pipewire/pipewire.git'
           + '@' + 'b741e0c74f5436f0c925f7741140db0efd32cf4e',
    'condition': 'checkout_linux',
  },

  'src/third_party/wireplumber/linux-amd64': {
    'packages': [
      {
        'package': 'chromium/third_party/wireplumber/linux-amd64',
        'version': 'yfe349C2e6pWCcu7DaSPJHNWdwDbNP9FfSQoV9j6A5UC',
      },
    ],

    'condition': 'checkout_linux',
    'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/autorolled',
              'version': 'E9rthy280mTyi24E5NtmsJyxWnItdtltZ3FhCA-7jUsC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/pthreadpool/src':
    Var('chromium_git') + '/external/github.com/google/pthreadpool.git' + '@' + '15a6644ba1c45f1acc16ac1e883efc3e56c6bed2',

  'src/third_party/xnnpack/src':
    Var('chromium_git') + '/external/github.com/google/XNNPACK.git' + '@' + 'edecd8af5e45bde84a36fa8220c80d6878824de2',

  'src/third_party/farmhash/src':
    Var('chromium_git') + '/external/github.com/google/farmhash.git' + '@' + '9d99331eb762e9ee22fd97d15a594525ad98310a',

  'src/third_party/ruy/src':
    Var('chromium_git') + '/external/github.com/google/ruy.git' + '@' + '0face1b4633b877082d26e3d4a75db394ef5fed3',

  'src/third_party/cpuinfo/src':
    Var('chromium_git') + '/external/github.com/pytorch/cpuinfo.git' + '@' + 'fd8b912165dfffccb4f652610a0a19afc51d98ee',

  'src/third_party/eigen3/src':
    Var('chromium_git') + '/external/gitlab.com/libeigen/eigen.git' + '@' + 'c7b9c4bc886e146132c85158f5a599b95ece92dd',

  'src/third_party/fp16/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '782eea126dc5c755827be751a099eb01826175cf',

  'src/third_party/gemmlowp/src':
    Var('chromium_git') + '/external/github.com/google/gemmlowp.git' + '@' + '16e8662c34917be0065110bfcd9cc27d30f52fdf',

  'src/third_party/fxdiv/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FXdiv.git' + '@' + '63058eff77e11aa15bf531df5dd34395ec3017c8',

  'src/third_party/neon_2_sse/src':
    Var('chromium_git') + '/external/github.com/intel/ARM_NEON_2_x86_SSE.git' + '@' + '8d623bb834cbd65b22dfe352800490223d78d32c',


  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  # === ANDROID_DEPS Generated Code End ===
}

hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python3',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/tools_webrtc/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python3',
        'src/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    # TODO(mbonadei): change to --arch=x86.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=i386'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    # TODO(mbonadei): change to --arch=mips.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mipsel'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    # TODO(mbonadei): change to --arch=x64.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=amd64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'src/build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary. Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'src/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python3', 'src/build/mac_toolchain.py'],
  },

  {
    'name': 'Download Fuchsia SDK from GCS',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python3',
      'src/build/fuchsia/update_sdk.py',
      '--cipd-prefix={fuchsia_sdk_cipd_prefix}',
      '--version={fuchsia_version}',
    ],
  },
  {
    'name': 'Download Fuchsia system images',
    'pattern': '.',
    'condition': 'checkout_fuchsia and checkout_fuchsia_product_bundles',
    'action': [
      'python3',
      'src/build/fuchsia/update_product_bundles.py',
      '{checkout_fuchsia_boot_images}',
    ],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'src/tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'src/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'src/tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'src/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/linux64/rc.sha1',
    ],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
  {
    'name': 'Generate component metadata for tests',
    'pattern': '.',
    'action': [
      'vpython3',
      'src/testing/generate_location_tags.py',
      '--out',
      'src/testing/location_tags.json',
    ],
  },
  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', 'src/vpython.toml',
                '-vpython-tool', 'install',
    ],
  },
  # Download remote exec cfg files
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg',
    'action': ['python3',
               'src/buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--quiet',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'action': ['python3',
               'src/build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
  {
    # Ensure we remove any file from disk that is no longer needed (e.g. after
    # hooks to native GCS deps migration).
    'name': 'remove_stale_files',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools/remove_stale_files.py',
        'src/third_party/test_fonts/test_fonts.tar.gz', # Remove after 20240901
    ],
  },
  {
    # Populate src/third_party/.git/info/exclude with nested src/third_party/*
    # DEPS paths so `gclient sync --reset --delete_unversioned_trees` does not
    # delete nested sub-repositories and the Windows toolchain on every build.
    'name': 'configure_third_party_exclude',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools_webrtc/configure_third_party_exclude.py',
    ],
  },
]

pre_deps_hooks = [
  {
    'name': 'configure_third_party_exclude',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools_webrtc/configure_third_party_exclude.py',
    ],
  },
]

recursedeps = [
  'src/buildtools',
  'src/third_party/instrumented_libs',
]

# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  "+external/webrtc/webrtc",  # Android platform build.
  "+libyuv",

  # These should eventually move out of here.
  "+common_types.h",

  "+WebRTC",
  "+api",
  "+modules/include",
  "+rtc_base",
  "+test",
  "+rtc_tools",

  # Abseil allowlist. Keep this in sync with abseil-in-webrtc.md.
  "+absl/algorithm/algorithm.h",
  "+absl/algorithm/container.h",
  "+absl/base/attributes.h",
  "+absl/base/config.h",
  "+absl/base/nullability.h",
  "+absl/base/macros.h",
  "+absl/base/no_destructor.h",
  "+absl/cleanup/cleanup.h",
  "+absl/container",
  "-absl/container/fixed_array.h",
  "+absl/crc",
  "+absl/functional/any_invocable.h",
  "+absl/functional/bind_front.h",
  "+absl/functional/overload.h",
  "+absl/memory/memory.h",
  "+absl/numeric/bits.h",
  "+absl/strings/ascii.h",
  "+absl/strings/escaping.h",
  "+absl/strings/match.h",
  "+absl/strings/str_cat.h",  # note - allowed for single argument version only
  "+absl/strings/str_replace.h",
  "+absl/strings/string_view.h",

  # Abseil flags are allowed in tests and tools.
  "+absl/flags",

  # Perfetto should be used through rtc_base/trace_event.h
  '-third_party/perfetto',
  '-perfetto',
  '-protos/perfetto',
]

specific_include_rules = {
  "webrtc_lib_link_test\\.cc": [
    "+media/engine",
    "+modules/audio_device",
    "+modules/audio_processing",
  ]
}
