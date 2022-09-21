{
  'includes': [
    'aworker_var.gypi',
  ],
  'include_dirs': [
    'src',
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(noslate_turf_dir)/include',
    '<(noslate_v8_dir)/include',
    '<(noslate_node_dir)/deps/icu-small/source/common',
    '<(noslate_node_dir)/deps/zlib',
  ],
  'defines': [
    'V8_DEPRECATION_WARNINGS',
    'V8_IMMINENT_DEPRECATION_WARNINGS',
    'AWORKER_ARCH="<(target_arch)"',
    'AWORKER_PLATFORM="<(OS)"',
    'AWORKER_TAG="-<(aworker_tag_v)"',
    'COMPRESS_IPV6_USE_INET',  # alpha test version
  ],
  'dependencies': [
    'aworker_proto_inl',
    '<(noslate_libyuv_dir)/yuv.gyp:libyuv',
    '<(noslate_build_dir)/gypfiles/protobuf.gyp:protobuf_lite',
    '<(noslate_v8_gyp):v8_snapshot',
    '<(noslate_v8_gyp):v8_libplatform',
    '<(noslate_build_dir)/gypfiles/cwalk.gyp:libcwalk',
    'aworker_v8.gyp:aworker_v8',
    'deps/modp_b64/modp_b64.gyp:modp_b64',
    'deps/argtable/argtable.gyp:argtable',
    'deps/cityhash/cityhash.gyp:cityhash',
    '<(noslate_zlib_gyp):zlib',
    '<(noslate_openssl_gyp):openssl',
  ],
  'conditions': [
    [
      '_type != "static_library" and aworker_use_curl=="true"', {
        'dependencies': [
          'src/binding/curl/curl.gyp:binding_curl',
        ],
      },
    ],
  ],
}
