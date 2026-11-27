# The tests of the Python tools under tools/ run with the Python of the
# build. They need neither a built program nor the network.
foreach name : [
  'changelog', 'harness', 'header_spacing', 'include_paths', 'layers',
  'othpatch_release', 'package_docs', 'package_files', 'release_index',
  'versioning',
]
  test(
    'tools/' + name,
    python,
    args: [files('test_' + name + '.py')],
    suite: ['unit', 'tools'],
  )
endforeach
