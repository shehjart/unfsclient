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
env.Append(CPPPATH = INCLUDE)

#libnfsclient and arpc
libnfs_env = env.Copy()
libnfsclient = SConscript('src/SConscript', exports = ['libnfs_env'])

#Test code environment
test_env = env.Copy()
test_env.Append(CPPPATH = INCLUDE)
nomeasurements = ARGUMENTS.get("nomeasurements", 0)

if int(nomeasurements):
	test_env.Append(CCFLAGS = '-D__NO_MEASUREMENTS__')

SConscript('tests/SConscript', exports = ['test_env', 'libnfsclient'])

#nfsclientd environment
nfsclientd_env = env.Copy()
nfsclientd_env.ParseConfig('pkg-config fuse --cflags --libs')
conf = Configure(nfsclientd_env)

if not conf.CheckLib('fuse'):
	print '\tlibfuse not found.'
	Exit(-1)

if not conf.CheckLib('pthread'):
	print '\tlibpthread not found.'
	Exit(-1)

if not conf.CheckCHeader('pthread.h'):
	print '\tpthread.h not found.'
	Exit(-1)

if not conf.CheckCHeader('fuse_lowlevel.h'):
	print '\tfuse_lowlevel.h not found.'
	Exit(-1)

if not conf.CheckFunc('daemon'):
	print '\tdaemon(3) function not available\n'
	Exit(-1)

nfsclientd_env = conf.Finish()

nodaemonize = ARGUMENTS.get("nodaemonize", 0)
if int(nodaemonize):
	nfsclientd_env.Append(CCFLAGS = '-D__NO_DAEMONIZE__')

nfsclientd_debug = ARGUMENTS.get('nfsclientd_debug', 0)
if int(nfsclientd_debug):
	nfsclientd_env.Append(CCFLAGS = '-D__NFSCLIENTD_DEBUG__')
	nfsclientd_env.Append(CCFLAGS = '-DDEBUG_PRINT')

nfsactor_debug = ARGUMENTS.get('nfsactor_debug', 0)
if int(nfsactor_debug):
	nfsclientd_env.Append(CCFLAGS = '-D__NFSACTOR_DEBUG__')
	nfsclientd_env.Append(CCFLAGS = '-DDEBUG_PRINT')

nfsclientdsource = ['src/nfsclientd.c', 'src/nfscd_ops.c', 'src/nfs3actor.c', 'src/debug_print.c']
nfsclientd_env.Program('nfsclientd', [nfsclientdsource, libnfsclient])

Default(None)
