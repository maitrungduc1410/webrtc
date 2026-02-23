# This file contains dependencies for WebRTC.

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'generate_location_tags',
]

vars = {
  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'chromium_revision': '80fdbfe13417d357d71d5cd8c24ccf4af6e1df70',

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  'chromium_git': 'https://chromium.googlesource.com',

  # Keep the Chromium default of generating location tags.
  'generate_location_tags': True,

  # ResultDB version
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',
  'fuchsia_version': 'version:31.20260204.7.1',
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
  # reclient CIPD package version
  'reclient_version': 're_client_version:0.185.0.db415f21-gomaip',
  # siso CIPD package version.
  'siso_version': 'git_revision:54be8bcbdd069237406d476362aeb261d6deb8c0',

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
    'https://chromium.googlesource.com/chromium/src/build@31ac3ef412f947513c552c7e81f860feb4de2ce0',
  'src/buildtools':
    'https://chromium.googlesource.com/chromium/src/buildtools@451e71432ac9f6a91d4d5604c29e0a5078e8c2e8',
  # Gradle 6.6.1. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': 'https://chromium.googlesource.com/external/github.com/gradle/gradle.git@f2d1fb54a951d8b11d25748e4711bec8d128d7e3',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': 'https://chromium.googlesource.com/chromium/src/ios@c68ca0692ee0da564f287410ab28ce926586445d',
    'condition': 'checkout_ios',
  },
  'src/testing':
    'https://chromium.googlesource.com/chromium/src/testing@45ac53e274c7fae5f221cca2b3d75e7328f071a2',
  'src/third_party':
    'https://chromium.googlesource.com/chromium/src/third_party@7361a7864218287d9a9e29af059e7a7d6c6356cc',

  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:2c775ed71ed9c7ceb11823ced47a1a2640883403',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:2c775ed71ed9c7ceb11823ced47a1a2640883403',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:2c775ed71ed9c7ceb11823ced47a1a2640883403',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win',
  },
  'src/buildtools/reclient': {
    'packages': [
      {
         # https://chrome-infra-packages.appspot.com/p/infra/rbe/client/
        'package': 'infra/rbe/client/${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
    # Reclient doesn't have linux-arm64 package.
    'condition': 'not (host_os == "linux" and host_cpu == "arm64")',
  },

  'src/third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'condition': 'not llvm_force_head_revision',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '9a5dfd23f6d37b21709327aa39562225c01a058cc209d1c629448d6d6d874ab2',
        'size_bytes': 57775784,
        'generation': 1770870606052091,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'cef6e9f2876cc28956213fd1cfb1431ed6bd9aca135b12ebd33e9d393595009d',
        'size_bytes': 14411824,
        'generation': 1770870606077571,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'efccb1da6950b4a83857ad5243196dd9d4cefee5869362abcc9d9422b059fff2',
        'size_bytes': 14638444,
        'generation': 1770870606182500,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '7d1648506f753035e0c96f9727abf58f0a275d8057be6fd3c231bf80ac60b2ac',
        'size_bytes': 2326952,
        'generation': 1770870606310079,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '103ba738fb6ebcd290e38458fdec8fa7b95bc3216af1e60ad260bc065b49d447',
        'size_bytes': 5798432,
        'generation': 1770870606263889,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '0230c7c9a625ceb2d2c650d995fb9cb5cdb6ab7c0c05042f560cc0cda441a2d0',
        'size_bytes': 54682532,
        'generation': 1770870608103936,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'a7af5d119637f67bff7f390923a78a712b1067157c019aebf2dcddbc4ed7b640',
        'size_bytes': 1015272,
        'generation': 1770870616072374,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '89bd1e39dc4114183a8cd931a9f5b3dc4fff5f697a133d91da2ecd2f82221774',
        'size_bytes': 14261468,
        'generation': 1770870608107521,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'd8c9c8f9a4e62422f2167b283da1e32d39978804543f7eaa08b903d4a0c580a2',
        'size_bytes': 15425576,
        'generation': 1770870608113120,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'a705f42d6e441950594986c0afb3f7b2e65cafa10eb1a5f40dc43c0a5d127af6',
        'size_bytes': 2372548,
        'generation': 1770870608249587,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '556b8abf3ec08c42769986ea8940ac7f09fa2cfe49328a2ccccee4641efce6c4',
        'size_bytes': 5703228,
        'generation': 1770870608115133,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '34102ff942cb0d7e2163fada5aeb53f5c2438d07d8caf5ed79970b133adc04be',
        'size_bytes': 45849908,
        'generation': 1770870617609650,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '11dfe174b262142ce8d2d9e810ff2a5c5ac5e44ccb135876825fc3ff24be1b35',
        'size_bytes': 12473944,
        'generation': 1770870617763889,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'a9d8847208d6411087464fb9d2f07d1a9a7617821ade0b6e00fb31e29934e12f',
        'size_bytes': 12883052,
        'generation': 1770870617857412,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'bf15d654a27132d07c5a03d6bb174f79449a88a34a08b9024e72a1b88127bdd0',
        'size_bytes': 1991372,
        'generation': 1770870618097417,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'aea68efe7aeab8eb4d2371e79e7dcb021e1448d8edd0d4851fa34907b93205a6',
        'size_bytes': 5446112,
        'generation': 1770870617921094,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'cca114f624db433c2ba6458a6de34e6e474bea379f483817dd5dc0a02c59df3c',
        'size_bytes': 49244312,
        'generation': 1770870628650736,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'de37a5f6e43f7d4ff7c7c8fe0ba7cd89bdd0cbe0370b171263f36ed372a7e3d0',
        'size_bytes': 14464644,
        'generation': 1770870628637623,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '3f2ad3f2adb92384ecdcb33385aa8beacd7f38d27bed6179e39b2e971469d028',
        'size_bytes': 2595252,
        'generation': 1770870636929948,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': 'b9f868e6be509a1900e7a8157cd29235245e0847804abb8d20fbc37772fbf802',
        'size_bytes': 14883160,
        'generation': 1770870628678782,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '30cb9e52d92731ed24cf08775aeb5eb0e4d2c1eb871daed1f344f6b4e635fe23',
        'size_bytes': 2477228,
        'generation': 1770870628999104,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-23-init-3706-gfc648683-1.tar.xz',
        'sha256sum': '7baba8d463b92d16c510d67be60eed89897ba76898121ea709264cbb41066af1',
        'size_bytes': 5859968,
        'generation': 1770870628750202,
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
        'object_name': 'Linux_x64/rust-toolchain-7dc2e92b83be02dc07f87be7e94266d6e48e5ca5-1-llvmorg-23-init-3706-gfc648683.tar.xz',
        'sha256sum': 'c14a211a9fcef604df768db22a4422553c791c4ad7e3f982cf00b9c0c745ccaa',
        'size_bytes': 266703680,
        'generation': 1770870599310471,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-7dc2e92b83be02dc07f87be7e94266d6e48e5ca5-1-llvmorg-23-init-3706-gfc648683.tar.xz',
        'sha256sum': 'dfd5f890b8d84eeee4cf5aa3ba9b9e2e070bb5bece134f979060c35b9286ed00',
        'size_bytes': 254634176,
        'generation': 1770870600934341,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-7dc2e92b83be02dc07f87be7e94266d6e48e5ca5-1-llvmorg-23-init-3706-gfc648683.tar.xz',
        'sha256sum': '4e0a8de0ef6e65d17f723193c44c7dc6495f2274e7a7818e9b84e9645281be7e',
        'size_bytes': 238113184,
        'generation': 1770870602823865,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-7dc2e92b83be02dc07f87be7e94266d6e48e5ca5-1-llvmorg-23-init-3706-gfc648683.tar.xz',
        'sha256sum': '1c783f691498d98769211f427fb3c6ef483ec1b710c6e661614f9bcec6a71c3d',
        'size_bytes': 404347608,
        'generation': 1770870604323594,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'src/third_party/clang-format/script':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@c2725e0622e1a86d55f14514f2177a39efea4a0e',
  'src/third_party/compiler-rt/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt.git@02ae8c1e75be8825a6c8c1cc74b4ea93933fb189',
  'src/third_party/libc++/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git@7ab65651aed6802d2599dcb7a73b1f82d5179d05',
  'src/third_party/libc++abi/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxxabi.git@8f11bb1d4438d0239d0dfc1bd9456a9f31629dda',
  'src/third_party/llvm-libc/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libc.git@a91d484f4c303b81ea50cbd396d200c08ebdcab3',
  'src/third_party/libunwind/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libunwind.git@17ccf7d110c5526cb77e93cfd8330f491fb2bf18',

  'src/third_party/test_fonts/test_fonts': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-fonts',
      'objects': [
          {
              'object_name': 'a28b222b79851716f8358d2800157d9ffe117b3545031ae51f69b7e1e1b9a969',
              'sha256sum': 'a28b222b79851716f8358d2800157d9ffe117b3545031ae51f69b7e1e1b9a969',
              'size_bytes': 33233004,
              'generation': 1746454415712520,
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
              'version': 'EpgkrtsLblLuw0BrsWCF0h_njBzIZsBNDxQ5VtA4s2UC',
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
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'vUP1cs7krVsNHbUJpwSDR7rvWuYLGzINVa9xopPpt74C',
          },
      ],
      'condition': 'checkout_android',
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

  'src/third_party/android_build_tools/dagger_compiler/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/dagger_compiler',
               'version': 'AC0DoTEXQf40KFt7hyCNSEJPrT9Rprw9zsZxNKdw7BQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone',
               'version': 'zUmV9Nh4JfIOMh9QU7LhIhPvodo_Aka-0IAIPIcLKVcC',
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
               'version': 'Ow8jpaRkHsJyhElyBOWWKBWl0KKNNuqbAE-cYWSPwQAC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },


  'src/third_party/android_build_tools/nullaway/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/nullaway',
               'version': 'rYm-c7W2XUUzeDYveLTl3YmrIqc483bjo6OB5rpQUrIC',
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
    'https://boringssl.googlesource.com/boringssl.git@3d72439f882dd6a9e8ddbc6d62b7c9557d9aac72',
  'src/third_party/breakpad/breakpad':
    'https://chromium.googlesource.com/breakpad/breakpad.git@79099fdf668ae097c9eca7052fd5c4c5de6c9098',
  'src/third_party/catapult':
    'https://chromium.googlesource.com/catapult.git@9fdb1d4c9d0f278ae34891b069d96ef89ea9cdf2',
  'src/third_party/ced/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/compact_enc_det.git@ba412eaaacd3186085babcd901679a48863c7dd5',
  },
  'src/third_party/colorama/src':
    'https://chromium.googlesource.com/external/colorama.git@3de9f013df4b470069d03d250224062e8cf15c49',
  'src/third_party/cpu_features/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/cpu_features.git@936b9ab5515dead115606559502e3864958f7f6e',
    'condition': 'checkout_android',
  },
  'src/third_party/crc32c/src':
    'https://chromium.googlesource.com/external/github.com/google/crc32c.git@d3d60ac6e0f16780bcfcc825385e1d338801a558',
  'src/third_party/depot_tools':
    'https://chromium.googlesource.com/chromium/tools/depot_tools.git@8fc02b1d3e48470d9d7fd0e6a68d42f0e38b3bac',
  'src/third_party/ffmpeg':
    'https://chromium.googlesource.com/chromium/third_party/ffmpeg.git@946d97db8d906277085e361892b7efda5152e2f1',
  'src/third_party/flatbuffers/src':
    'https://chromium.googlesource.com/external/github.com/google/flatbuffers.git@a86afae9399bbe631d1ea0783f8816e780e236cc',
  'src/third_party/grpc/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/grpc/grpc.git@c485bf9b093bd32670b6af8a3a20eae1e2604602',
  },
  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': 'https://chromium.googlesource.com/external/fontconfig.git@d62c2ab268d1679335daa8fb0ea6970f35224a76',
      'condition': 'checkout_linux',
  },
  'src/third_party/freetype/src':
    'https://chromium.googlesource.com/chromium/src/third_party/freetype2.git@e3a0652b6d706ee1ce77d4dda606b6597dd8b5c4',
  'src/third_party/harfbuzz-ng/src':
    'https://chromium.googlesource.com/external/github.com/harfbuzz/harfbuzz.git@fa2908bf16d2ccd6623f4d575455fea72a1a722b',
  'src/third_party/google_benchmark/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/benchmark.git@188e8278990a9069ffc84441cb5a024fd0bede37',
  },
  'src/third_party/libpfm4/src':
    Var('chromium_git') + '/external/git.code.sf.net/p/perfmon2/libpfm4.git' + '@' + '964baf9d35d5f88d8422f96d8a82c672042e7064',
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/gtest-parallel':
    'https://chromium.googlesource.com/external/github.com/google/gtest-parallel@cd488bdedc1d2cffb98201a17afc1b298b0b90f1',
  'src/third_party/google-truth/src': {
      'url': 'https://chromium.googlesource.com/external/github.com/google/truth.git@33387149b465f82712a817e6744847fe136949b3',
      'condition': 'checkout_android',
  },
  'src/third_party/googletest/src':
    'https://chromium.googlesource.com/external/github.com/google/googletest.git@4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  'src/third_party/icu': {
    'url': 'https://chromium.googlesource.com/chromium/deps/icu.git@a86a32e67b8d1384b33f8fa48c83a6079b86f8cd',
  },
  'src/third_party/jdk/current': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/linux-amd64',
              'version': '2iiuF-nKDH3moTImx2op4WTRetbfhzKoZhH7Xo44zGsC',
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
 'src/third_party/jsoncpp/source':
    'https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp.git@42e892d96e47b1f6e29844cc705e148ec4856448', # from svn 248
  'src/third_party/junit/src': {
    'url': 'https://chromium.googlesource.com/external/junit.git@0eb5ce72848d730da5bd6d42902fdd6a8a42055d',
    'condition': 'checkout_android',
  },
  'src/third_party/kotlin_stdlib/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': 'MQrhfTOsQoBh8QajYKNMkPBvzh7bqyv31z7HPbnbo_QC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/kotlinc/current': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlinc',
              'version': '1wGbWZPhnUbDtQCvcw74lP1g0ceq55pEkvO5hZ3DRLcC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/libFuzzer/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git@bea408a6e01f0f7e6c82a43121fe3af4506c932e',
  'src/third_party/fuzztest/src':
    'https://chromium.googlesource.com/external/github.com/google/fuzztest.git@7636e7cbf8281e049950f7c229fce46e52011c21',
  'src/third_party/libjpeg_turbo':
    'https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git@6bb85251a8382b5e07f635a981ac685cc5ab5053',
  'src/third_party/libsrtp':
    'https://chromium.googlesource.com/chromium/deps/libsrtp.git@a52756acb1c5e133089c798736dd171567df11f5',
  'src/third_party/dav1d/libdav1d':
    'https://chromium.googlesource.com/external/github.com/videolan/dav1d.git@b546257f770768b2c88258c533da38b91a06f737',
  'src/third_party/libaom/source/libaom':
    'https://aomedia.googlesource.com/aom.git@39606bf4bae2befef8c32bcdfab402226bb33783',
  'src/third_party/libgav1/src':
    Var('chromium_git') + '/codecs/libgav1.git' + '@' + '40f58ed32ff39071c3f2a51056dbc49a070af0dc',
  'src/third_party/libunwindstack': {
      'url': 'https://chromium.googlesource.com/chromium/src/third_party/libunwindstack.git@0928ad0d25e4af07c8be5ab06d0ca584f9f4ceb5',
      'condition': 'checkout_android',
  },
  'src/third_party/perfetto':
    Var('chromium_git') + '/external/github.com/google/perfetto.git' + '@' + '601cac04b0757f1132c64ace99205522d5cabb2d',
  'src/third_party/protobuf-javascript/src':
    Var('chromium_git') + '/external/github.com/protocolbuffers/protobuf-javascript' + '@' + 'e6d763860001ba1a76a63adcff5efb12b1c96024',
  'src/third_party/libvpx/source/libvpx':
    'https://chromium.googlesource.com/webm/libvpx.git@a0ac06f5d3c83f890cb502c80470ef0502343fec',
  'src/third_party/libyuv':
    'https://chromium.googlesource.com/libyuv/libyuv.git@6067afde563c3946eebd94f146b3824ab7a97a9c',
  'src/third_party/lss': {
    'url': 'https://chromium.googlesource.com/linux-syscall-support.git@29164a80da4d41134950d76d55199ea33fbb9613',
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': 'https://chromium.googlesource.com/external/mockito/mockito.git@04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' + '@' + '69015643b3f68dbd438c010439c59adc52cac808',
    'condition': 'checkout_instrumented_libraries',
  },

  # Used by boringssl.
  'src/third_party/nasm': {
      'url': 'https://chromium.googlesource.com/chromium/deps/nasm.git@af5eeeb054bebadfbb79c7bcd100a95e2ad4525f'
  },

  'src/third_party/openh264/src':
    'https://chromium.googlesource.com/external/github.com/cisco/openh264@652bdb7719f30b52b08e506645a7322ff1b2cc6f',

  'src/third_party/re2/src':
    'https://chromium.googlesource.com/external/github.com/google/re2.git@972a15cedd008d846f1a39b2e88ce48d7f166cbd',

  'src/third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'OdXvkYaHnRN6eudfIbRYWbc4JTiLo6bJKvftjyuQt2UC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  # This duplication is intentional, so we avoid updating the r8.jar used by
  # dexing unless necessary, since each update invalidates all incremental
  # dexing and unnecessarily slows down all bots.
  'src/third_party/r8/d8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'dKavKsYt7Uj8fRmoAALj-uumULBl43j-NHRUZ1YvnxUC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/requests/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/kennethreitz/requests.git@c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
    'condition': 'checkout_android',
  },
  'src/tools':
    'https://chromium.googlesource.com/chromium/src/tools@6580288c8d453357aaf4e3388b62aa423b0bf53c',

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
        'package': 'chromium/third_party/android_toolchain/android_toolchain',
        'version': 'KXOia11cm9lVdUdPlbGLu8sCz6Y4ey_HV2s8_8qeqhgC',
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },

  'src/third_party/androidx/cipd': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': 'DLUFuZ1z4MFDK54yAzN2lScwHZlUoJssu_cnrpDbqwYC',
      },
    ],
    'condition': 'checkout_android and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'vWr5VRbtPlKQeEMNOksUpsAqeH2yaPhBRf3C59yVhzQC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/36.1.0',
              'version': '-jLl4Ibk_WmgTsZaP-ueQwZDhBwkWf5BsQ4UNrkzXF0C',
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
              'package': 'chromium/third_party/android_sdk/public/platforms/android-36.1',
              'version': 'gxwLT70eR_ObwZJzKK8UIS-N549yAocNTmc0JHgO7gUC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools/linux',
              'version': 'LZa8CWNVWS6UUQgQ7IJdFCqRV1Bmx2-alTNqEDJpJkcC',
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
              'package': 'chromium/third_party/robolectric',
              'version': 'dr-aJxRAPYDTBJXnjfht-bdxyywD6BP1lrcjZZPnRG0C',
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
    Var('chromium_git') + '/external/github.com/tensorflow/tensorflow.git' + '@' + '38dd09551686ad00bc837f383b3a1db282079498',

  'src/third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'ymsdRx1sSajlZKbTHsqqquqR8lYueFUJCPwDCXmIfMEC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/zstd/src': {
    'url': Var('chromium_git') + '/external/github.com/facebook/zstd.git' + '@' + '1168da0e567960d50cba1b58c9b0ba047ece4733',
    'condition': 'checkout_android',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/cas/${{platform}}',
          'version': 'git_revision:21504d3595ac903ef60bac732d885cd8e6ed92ba',
        },
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': 'git_revision:21504d3595ac903ef60bac732d885cd8e6ed92ba',
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': 'git_revision:21504d3595ac903ef60bac732d885cd8e6ed92ba',
        }
      ],
      'dep_type': 'cipd',
  },

  'src/third_party/pipewire/linux-amd64': {
    'packages': [
      {
        'package': 'chromium/third_party/pipewire/linux-amd64',
        'version': 'WccKnxvPRn8TBwQ4FUxoHaZRKvHXh0qJjODccy3gPTAC',
      },
      {
        'package': 'chromium/third_party/wireplumber/linux-amd64',
        'version': 'yfe349C2e6pWCcu7DaSPJHNWdwDbNP9FfSQoV9j6A5UC',
      },
    ],

    'condition': 'checkout_linux',
    'dep_type': 'cipd',
  },

  'src/third_party/android_deps/autorolled/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/autorolled',
              'version': 'Lskjo0vFA222MYDpjYl71PwoWG_9mb-F_v3RTHvE-LkC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },
  'src/third_party/pthreadpool/src':
    Var('chromium_git') + '/external/github.com/google/pthreadpool.git' + '@' + '9003ee6c137cea3b94161bd5c614fb43be523ee1',

  'src/third_party/xnnpack/src':
    Var('chromium_git') + '/external/github.com/google/XNNPACK.git' + '@' + 'e8aad3cc4ffb7259c4aab6dfb922bfbef0ef8be0',

  'src/third_party/farmhash/src':
    Var('chromium_git') + '/external/github.com/google/farmhash.git' + '@' + '816a4ae622e964763ca0862d9dbd19324a1eaf45',

  'src/third_party/ruy/src':
    Var('chromium_git') + '/external/github.com/google/ruy.git' + '@' + '2af88863614a8298689cc52b1a47b3fcad7be835',

  'src/third_party/cpuinfo/src':
    Var('chromium_git') + '/external/github.com/pytorch/cpuinfo.git' + '@' + '7364b490b5f78d58efe23ea76e74210fd6c3c76f',

  'src/third_party/eigen3/src':
    Var('chromium_git') + '/external/gitlab.com/libeigen/eigen.git' + '@' + '667cabe3aa54d53d6c542950f6d3876c7f0ba58f',

  'src/third_party/fp16/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '3d2de1816307bac63c16a297e8c4dc501b4076df',

  'src/third_party/gemmlowp/src':
    Var('chromium_git') + '/external/github.com/google/gemmlowp.git' + '@' + '16e8662c34917be0065110bfcd9cc27d30f52fdf',

  'src/third_party/fxdiv/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FXdiv.git' + '@' + '63058eff77e11aa15bf531df5dd34395ec3017c8',

  'src/third_party/neon_2_sse/src':
    Var('chromium_git') + '/external/github.com/intel/ARM_NEON_2_x86_SSE.git' + '@' + '662a85912e8f86ec808f9b15ce77f8715ba53316',


  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'src/third_party/android_deps/cipd/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework',
              'version': 'version:2@4.0.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy_agent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jsoup_jsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jsoup_jsoup',
              'version': 'version:2@1.15.1.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_android',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_core',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_subclass': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_subclass',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_objenesis_objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_objenesis_objenesis',
              'version': 'version:2@3.3.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

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
                '-vpython-spec', 'src/.vpython3',
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
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
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
  "+absl/cleanup/cleanup.h",
  "+absl/container",
  "-absl/container/fixed_array.h",
  "+absl/functional/any_invocable.h",
  "+absl/functional/bind_front.h",
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
