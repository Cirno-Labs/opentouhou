foreach part : ['mdrv_data', 'mdrv', 'pmd', 'opna', 'vsync', 'master_rng', 'math16', 'bfnt', 'logo1', 'pf', 'pi', 'sprite_sheet', 'pmd_data', 'pmd_effect', 'pmd_effect_stream', 'pmd_fm', 'pmd_lfo', 'pmd_part', 'pmd_pitch', 'pmd_ssg', 'pmd_drums', 'pmd_sequence']
  test(
    'pc98/' + part,
    executable(
      'test_pc98_' + part,
      'test_pc98_' + part + '.c',
      c_args: tests_include,
      implicit_include_directories: false,
      dependencies: [pc98_dep, test_deps] + (part in ['mdrv_data', 'sprite_sheet', 'pmd_data'] ? [sdl3] : []),
    ),
    suite: 'unit',
    env: {'TEST_DIR': tests_tmp / 'pc98' / part},
  )
endforeach
test(
  'pc98/mdrv_malformed',
  python,
  args: [
    expect_fatal,
    'Invalid MDRV opcode d0 at 0',
    executable(
      'test_pc98_mdrv_malformed',
      'test_pc98_mdrv_malformed.c',
      c_args: tests_include,
      implicit_include_directories: false,
      dependencies: [pc98_dep, sdl3],
    ),
  ],
  suite: 'unit',
)
