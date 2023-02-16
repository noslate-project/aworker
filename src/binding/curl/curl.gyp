{
  'targets': [
    {
      'target_name': 'binding_curl',
      'binding_name': 'curl',
      'type': 'static_library',
      'includes': [
        '../binding.gypi',
      ],
      'sources': [
        'binding.cc',
        'curl_easy.cc',
        'curl_multi.cc',
        'curl_version.cc',
      ],
      'dependencies': [
        '<(noslate_cares_gyp):cares',
        '<(noslate_build_dir)/gypfiles/curl.gyp:libcurl',
      ],
    },
  ]
}
