{
  # NOTE: includes cannot contain any variable references.
  'includes': [
    '../node/tools/v8_gypfiles/features.gypi',
  ],
  'targets': [
    {
      'target_name': 'aworker_v8',
      'type': 'static_library',
      'dependencies': [
        '<(noslate_v8_gyp):v8_snapshot',
        # [GYP] added explicitly, instead of inheriting from the other deps
        '<(noslate_v8_gyp):torque_generated_definitions',
      ],
      'variables': {
        'generate_bytecode_output_root': '<(SHARED_INTERMEDIATE_DIR)/generate-bytecode-output-root',
        'torque_output_root': '<(SHARED_INTERMEDIATE_DIR)/torque-output-root',
      },
      'include_dirs': [
        '<(noslate_v8_dir)/include',
        '<(noslate_v8_dir)/',
        '<(generate_bytecode_output_root)',
        '<(torque_output_root)',
        '<(noslate_node_dir)/deps/icu-small/source/common',
      ],
      'sources': [
        'src/aworker_v8.cc',
      ],
    },  # aworker_v8
  ]
}
