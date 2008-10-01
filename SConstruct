import commands
import os

#Global params
env = Environment(CC = 'gcc')
env.Append(CCFLAGS = '-Wall')
env.Append(ENV = os.environ)

debug = ARGUMENTS.get('debug', 0)
if int(debug):
	env.Append(CCFLAGS = '-ggdb3')

debug_print = ARGUMENTS.get('debug_print', 0)
if int(debug_print):
	env.Append(CCFLAGS = '-DDEBUG_PRINT')

INCLUDE = ['#include']

libnfs_env = env.Copy()
libnfs_env.Append(CPPPATH = INCLUDE)

libnfsclient = SConscript('src/SConscript', exports = ['libnfs_env'])

#Test code environment
test_env = env.Copy()
test_env.Append(CPPPATH = INCLUDE)
SConscript('tests/SConscript', exports = ['test_env', 'libnfsclient'])

Default(None)
