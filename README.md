# JPTODO - Telegram TODO bot in C

Simple telegram TODO bot based on [antirez's botlib](https://github.com/antirez/botlib).

# Build and run

Dependencies: libsqlite3 libcurl

And just run:

```bash
make && ./jptodo --apikey <your_api_key>
```

More advanced command line arguements are shown in the original repo.

# Usage

There is a topic related to each todo content.

Available bot commands:

```
/add <topic> <todo contents ...>          Add a todo content related to topic.
/ls                                       Show all todo contents.
/ls  <topic>                              Show todo contents under a topic.
/del <topic> <number 1> <number 2> ...    Finish a todo entry in given topic.
```
