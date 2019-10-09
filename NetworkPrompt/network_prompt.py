'''
Python implementation of the NetworkPrompt example.

`import network_prompt` to enable the prompts in Python 3.8.
'''

import sys

def network_prompt_hook(event, args):
    # Only care about 'socket.' events
    if not event.startswith("socket."):
        return

    if event == "socket.getaddrinfo":
        msg = "WARNING: Attempt to resolve {}:{}".format(args[0], args[1])
    elif event == "socket.connect":
        addro = args[0]
        msg = "WARNING: Attempt to connect {}:{}".format(addro[0], addro[1])
    else:
        msg = "WARNING: {} (event not handled)".format(event)

    ch = input(msg + ". Continue [Y/n]\n")
    if ch == 'n' or ch == 'N':
        sys.exit(1)

sys.addaudithook(network_prompt_hook)
print("Enabled prompting on network access.")
