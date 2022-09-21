{
  'targets': [
    {
      'target_name': 'cityhash',
      'type': 'static_library',
      'direct_dependent_settings': {
        'include_dirs': [
          './'
        ]
      },
      'sources': [
        'city.cc'
      ]
    }
  ]
}
