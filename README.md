# Simple Perftest

I've hacked up a simple `dperftest` because it was faster to do so than looking for an existing solution.

## Objective

This perftest simply measures the performance of sending one POST request per a line of the input file. Optionally, it compares the results against the golden set.

The requests are treated as one request per line. (Presumably, they are one JSON per line, although I don't even set `Content-Type` -- D.K.)

The golden responses should also be one response per line.

Also, this service can not make HTTPS requests, only HTTP ones.

## Installation

```
git clone https://github.com/dkorolev/dperftest.git
(cd dperftest; make release)
(cd dperftest; sudo make install)
```

The `make install` commands copies the freshly built `dperftest` binary into `/usr/local/bin`.
