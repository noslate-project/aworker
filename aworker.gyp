{
  'includes': [
    'aworker_var.gypi',
  ],
  'target_defaults': {
    'conditions': [
      [ 'OS!="win"', { # POSIX
        'defines': [ '__POSIX__' ],
      }],
      [ 'OS=="mac"', {
        # linking Corefoundation is needed since certain OSX debugging tools
        # like Instruments require it for some features
        'libraries': [ '-framework CoreFoundation' ],
        'defines!': [
          'AWORKER_PLATFORM="mac"',
        ],
        'defines': [
          # we need to use node's preferred "darwin" rather than gyp's preferred "mac"
          'AWORKER_PLATFORM="darwin"',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'libaworker',
      'type': 'static_library',
      'sources': [
        '<@(aworker_source_files)',
      ],
      'dependencies': [
        'js2c',
        'command_parser-options',
        'aworker_proto',
      ],
      'includes': [
        'aworker.gypi',
        'src/inspector/aworker_inspector.gypi',
      ],
      'direct_dependent_settings': {
        'xcode_settings': {
          'OTHER_LDFLAGS': [
            '-Wl,-force_load,<(aworker_obj_dir)/<(STATIC_LIB_PREFIX)aworker<(STATIC_LIB_SUFFIX)',
          ],
        },
        'ldflags': [
          '-Wl,--whole-archive',
          '<(aworker_obj_dir)/<(STATIC_LIB_PREFIX)aworker<(STATIC_LIB_SUFFIX)',
          '-Wl,--no-whole-archive',
        ],
      },
    },
    {
      'target_name': 'aworker',
      'type': 'executable',
      'sources': [
        'src/aworker_main.cc',
      ],
      'dependencies': [
        'libaworker',
      ],
      'actions': [
        {
          'action_name': 'aworker.shell',
          'inputs': [
            'tools/shell.js',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/aworker.shell',
          ],
          'action': [
            'cp',
            'tools/shell.js',
            '<(PRODUCT_DIR)/aworker.shell',
          ],
        },
      ],
      'conditions': [
        ['aworker_use_snapshot=="true"', {
          'dependencies': [
            'run_aworker_mksnapshot',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/aworker_snapshot.cc',
          ],
        }, {
          'sources': [
            'src/snapshot/embedded_snapshot_data_stub.cc',
          ],
        }],
      ],
      'includes': [
        'aworker.gypi',
      ],
    },
    {
      'target_name': 'aworker_cctest',
      'type': 'executable',
      'sources': [
        '<@(aworker_cctest_source_files)',
      ],
      'dependencies': [
        'libaworker',
        '<(noslate_gtest_gyp):gtest',
        '<(noslate_gtest_gyp):gtest_main',
      ],
      'conditions': [
        ['aworker_use_snapshot=="true"', {
          'dependencies': [
            'run_aworker_mksnapshot',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/aworker_snapshot.cc',
          ],
        }, {
          'sources': [
            'src/snapshot/embedded_snapshot_data_stub.cc',
          ],
        }],
      ],
      'includes': [
        'aworker.gypi',
      ],
    },
    {
      'target_name': 'js2c',
      'type': 'none',
      'sources': [
        'tools/js2c.js',
        '<@(aworker_library_files)',
      ],
      'actions': [
        {
          'action_name': 'proto',
          'inputs': [
            'tools/js2c.js',
            '<@(aworker_library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/aworker_native_modules.cc',
          ],
          'action': [
            '<(noslate_build_dir)/toolchains/node/bin/node',
            'tools/js2c.js',
            '<(SHARED_INTERMEDIATE_DIR)/aworker_native_modules.cc',
            '<@(aworker_library_files)'
          ]
        }
      ],
    },
    {
      'target_name': 'run_aworker_mksnapshot',
      'type': 'none',
      'sources': [
        '<@(aworker_library_files)',
      ],
      'conditions': [
        ['aworker_snapshot_main!=""', {
          'actions': [
            {
              'action_name': 'run_aworker_mksnapshot',
              'inputs': [
                '<(mksnapshot_exec)',
                '<(aworker_snapshot_main)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/aworker_snapshot.cc',
              ],
              'action': [
                '<(mksnapshot_exec)',
                '<@(_outputs)',
                '--build-snapshot',
                '<(aworker_snapshot_main)',
              ],
            },
          ],
        }, {
          'actions': [
            {
              'action_name': 'run_aworker_mksnapshot',
              'inputs': [
                '<(mksnapshot_exec)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/aworker_snapshot.cc',
              ],
              'action': [
                '<(mksnapshot_exec)',
                '<@(_outputs)',
                '--build-snapshot',
              ],
            },
          ],
        }],
      ],
      'dependencies': [
        'aworker_mksnapshot',
      ],
    },
    {
      'target_name': 'aworker_mksnapshot',
      'type': 'executable',
      'sources': [
        'tools/snapshot/mksnapshot.cc',
        'src/snapshot/embedded_snapshot_data_stub.cc',
      ],
      'dependencies': [
        'libaworker',
        '<(noslate_v8_gyp):v8_snapshot',
        '<(noslate_v8_gyp):v8_libplatform',
      ],
      'includes': [
        'aworker.gypi',
      ],
    },
    {
      'target_name': 'command_parser-options',
      'type': 'none',
      'sources': [
        'tools/command_parser_options.js',
        'src/command_parser-options.json'
      ],
      'actions': [{
        'action_name': 'proto',
        'inputs': [
          'tools/command_parser_options.js',
          'src/command_parser-options.json'
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/command_parser-options.h'
        ],
        'action': [
          '<(noslate_build_dir)/toolchains/node/bin/node',
          'tools/command_parser_options.js',
          '<(SHARED_INTERMEDIATE_DIR)/command_parser-options.h'
        ]
      }]
    },
    {
      'target_name': 'aworker_proto',
      'type': 'none',
      'sources': [
        '<@(proto_files)',
      ],
      'rules': [
        {
          'rule_name': 'proto',
          'process_outputs_as_sources': 1,
          'extension': 'proto',
          'inputs': [],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/proto/<(RULE_INPUT_ROOT).pb.cc',
            '<(SHARED_INTERMEDIATE_DIR)/proto/<(RULE_INPUT_ROOT).pb.h',
          ],
          'action': [
            '<(noslate_build_dir)/toolchains/protoc/protoc',
            '--cpp_out=<(SHARED_INTERMEDIATE_DIR)/proto',
            '-Isrc/proto',
            'src/proto/<(RULE_INPUT_ROOT).proto'
          ],
          'message': 'protoc <(RULE_INPUT_ROOT)'
        }
      ]
    },
    {
      'target_name': 'aworker_proto_inl',
      'type': 'none',
      'sources': [
        '<@(proto_v8_files)',
        'tools/protoc_v8.js'
      ],
      'actions': [
        {
          'action_name': 'protoc',
          'inputs': [
            '<@(proto_v8_files)',
            'tools/protoc_v8.js'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/proto/proto-inl.pb.h'
          ],
          'action': [
            '<(noslate_build_dir)/toolchains/node/bin/node',
            'tools/protoc_v8.js',
            '--out',
            '<@(_outputs)',
            '<@(proto_v8_files)',
          ]
        }
      ]
    },
    {
      'target_name': 'aworker_test_proto',
      'type': 'none',
      'sources': [
        'test/cctest/proto/test.proto',
      ],
      'rules': [
        {
          'rule_name': 'proto',
          'process_outputs_as_sources': 1,
          'extension': 'proto',
          'inputs': [],
          'outputs': [
            'test/cctest/proto/<(RULE_INPUT_ROOT).pb.cc',
            'test/cctest/proto/<(RULE_INPUT_ROOT).pb.h',
          ],
          'action': [
            '<(noslate_build_dir)/toolchains/protoc/protoc',
            '--cpp_out=./test/cctest/proto',
            '-Itest/cctest/proto',
            'test/cctest/proto/<(RULE_INPUT_ROOT).proto'
          ],
          'message': 'protoc <(RULE_INPUT_ROOT)'
        }
      ]
    },
  ]
}
