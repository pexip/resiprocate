
shared_test_sources = [
  'CommandLineParser.cxx',
  'UserAgent.cxx',
  'RegEventClient.cxx',
  'basicClientCall.cxx',
  'basicClientCmdLineParser.cxx',
  'basicClientUserAgent.cxx',
]

test_sources = [
  'basicRegister.cxx',
  'BasicCall.cxx',
  'basicMessage.cxx',
  'basicClient.cxx',
]

foreach fname : test_sources
  test_name = fname.split('.').get(0).underscorify()

  exe = executable(test_name,
    [fname, shared_test_sources],
    include_directories : base_dir,
    link_with : [libdum_lib],
    #dependencies : [winsock2_dep, winmm_dep],
    install : true)

  test(test_name, exe, timeout : 100, is_parallel : false, suite: test_name)
endforeach
