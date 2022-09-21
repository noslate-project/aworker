{
  'targets': [
    {
      'target_name': 'aworker_inspector_protocol',
      'type': 'none',
      'variables': {
        'tools_path': '../../tools/',
        'intermediate_path': '<(SHARED_INTERMEDIATE_DIR)/aworker-inspector-output-root/',
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(intermediate_path)',
        ],
      },
      'actions': [
        {
          'action_name': 'concatenate_protocols',
          'inputs': [
            '<(noslate_v8_dir)/include/js_protocol.pdl',
          ],
          'outputs': [
            '<(intermediate_path)/inspector_protocol.json',
          ],
          'action': [
            '<(python)',
            '<(tools_path)/inspector_protocol/concatenate_protocols.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
        {
          'action_name': 'v8_inspector_compress_protocol_json',
          'inputs': [
            '<(intermediate_path)/inspector_protocol.json',
          ],
          'outputs': [
            '<(intermediate_path)/v8_inspector_protocol_json.h',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            '<(python)',
            '<(tools_path)/compress_json.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    }
  ]
}
