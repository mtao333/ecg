[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-c66648af7eb3fe8bc4f294546bfd86ef473780cde1dea487d3c4ff354943c9ae.svg)](https://classroom.github.com/online_ide?assignment_repo_id=8193569&assignment_repo_type=AssignmentRepo)
# <p align="center">LE6: Adding Signals to PA3<p>

**Introduction**

The objective of this exercise is to give you hands-on experience creating and using signals and their handlers. The server will now periodically send messages to the client denoting percent complete, thread activity, and other diagnostic information to the client on the control channel. The client will process these messages with signals and their handlers to send these messages to the user.

Based on PA3, the server will periodically send diagnostic information to the client on the control channel. The first byte of the message will be the message code, that specifies which signal to alert. The rest of the packet is the message details to print on the client’s console. Format ‘<code><msg>’.

These messages will be sent through the control channel. Note: you cannot use the control channel for anything other than reading these signal messages. This may mean you need to rework how you create the file thread.

**Tasks**

- [ ] Make sure the control channel is only used to monitor for signal messages
- [ ] Write signal thread function to monitor signals
- [ ] Write signal handler for Info signals
- [ ] Write signal handler for Error signals
- [ ] Join all threads
- [ ] Close all channels

See the LE6 module on Canvas for further details and assistance.
