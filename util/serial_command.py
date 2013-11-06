from optparse import OptionParser
import pexpect
import signal
import sys
from time import sleep

terminal = None

class ExitStatusError(Exception):
    """Program had a non-zero exit status"""
    pass

def open_terminal(tty, baud):
    global terminal
    terminal = pexpect.spawn('screen %s %d' % (tty, baud))

def close_terminal():
    global terminal
    if terminal:
        terminal.sendcontrol('a')
        terminal.sendline('\y')
        terminal = None

def serial_command(tty, baud, cmd, check_exit_status=True):
    global terminal
    open_terminal(tty, baud) 
    response = None

    try:
        print 'Waiting for prompt...'
        terminal.sendcontrol('c')
        terminal.expect('~ # ')
    except pexpect.EOF:
        print "error: terminal busy"
        close_terminal()
        raise
    except pexpect.TIMEOUT:
        print "error: shell timeout"
        close_terminal()
        raise

    try:
        print 'Running `%s`...' % cmd
        terminal.sendline(cmd)
        terminal.expect('~ # ')
        response = terminal.before
    except pexpect.TIMEOUT:
        print "error: test timeout"
        close_terminal()
        raise

    if check_exit_status:
        try:
            print 'Getting exit status...'
            terminal.sendline('echo $?-')
            terminal.expect('~ # ')
            exit_status = int(terminal.before.split('\r\n')[1][:-1])
            if exit_status:
                raise ExitStatusError, response
        except pexpect.TIMEOUT:
            print "error: exit status timeout"
            close_terminal()
            raise
        except:
            close_terminal()
            print 'error: exit was not 0'
            raise

    close_terminal()
    return response

def serial_uboot(tty, baud, cmd, reset):
    global terminal
    serial_command(tty, baud, 'reboot', check_exit_status=False)
    sleep(2)

    open_terminal(tty, baud)

    try:
        print 'Waiting for autoboot...'
        terminal.expect('Hit any key to stop autoboot:')
        terminal.sendline()
    except pexpect.TIMEOUT:
        print "error: no stop autoboot"
        close_terminal()
        raise

    try:
        print 'Waiting for uboot prompt...'
        terminal.expect('A2F500-SOM> ')
        terminal.sendline(cmd)
    except pexpect.TIMEOUT:
        print "error: uboot timeout"
        close_terminal()
        raise

    if reset:
        try:
            print 'Waiting for uboot reset prompt...'
            terminal.expect('A2F500-SOM> ')
            terminal.sendline('reset')
        except pexpect.TIMEOUT:
            print "error: uboot reset timeout"
            close_terminal()
            raise

    try:
        print 'Waiting for prompt...'
        terminal.expect('~ # ')
    except pexpect.TIMEOUT:
        print "error: prompt timeout"
        close_terminal()
        raise

    response = terminal.before
    close_terminal()
    return response

if __name__ == '__main__':
    def interrupt_handler(signal, frame):
        close_terminal()
        exit(1)
        
    signal.signal(signal.SIGINT, interrupt_handler)

    parser = OptionParser()
    parser.add_option('-t', '--tty',
        default='/dev/ttyUSB0',
        help='tty, e.g. /dev/ttyUSB0')
    parser.add_option('-b', '--baud',
        type="int",
        default=115200,
        help='baud rate, default is 115200')
    parser.add_option('-u', '--u-boot',
        default=False,
        action='store_true',
        dest='uboot',
        help='Restart and execute a u-boot command')
    parser.add_option('-r', '--reset',
        default=False,
        action='store_true',
        dest='reset',
        help='Add running reset; only valid with uboot')

    options, args = parser.parse_args()

    if options.uboot:
        response = serial_uboot(options.tty, options.baud, ' '.join(args),
            options.reset)
    else:
        response = serial_command(options.tty, options.baud, ' '.join(args))
    print response
    sys.exit(0)
