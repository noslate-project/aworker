{
  'includes': [
    '../../aworker_var.gypi',
  ],
  'include_dirs': [
    '../',
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(noslate_v8_dir)/include',
  ],
  'defines': [
    'V8_DEPRECATION_WARNINGS',
    'V8_IMMINENT_DEPRECATION_WARNINGS',
    'AWORKER_ARCH="<(target_arch)"',
    'AWORKER_PLATFORM="<(OS)"',
    'AWORKER_TAG="-<(aworker_tag_v)"',
  ],
  'dependencies': [
    '<(noslate_libyuv_dir)/yuv.gyp:libyuv',
    # TODO(chengzhong.wcz): maybe <(noslate_v8_gyp):v8 is fine.
    '<(noslate_v8_gyp):v8_snapshot',
  ],
  'direct_dependent_settings': {
    'xcode_settings': {
      'OTHER_LDFLAGS': [
        '-Wl,-force_load,<(aworker_obj_dir)/<(STATIC_LIB_PREFIX)<(_target_name)<(STATIC_LIB_SUFFIX)',
      ],
    },
    'ldflags': [
      '-Wl,--whole-archive',
      '<(aworker_obj_dir)/src/binding/<(_binding_name)/<(STATIC_LIB_PREFIX)<(_target_name)<(STATIC_LIB_SUFFIX)',
      '-Wl,--no-whole-archive',
    ],
  },
}
