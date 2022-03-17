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

## Example

The `make release` command above builds a few more binaries. Among them is a simple example server, and a simple test data generator for it. To see `dperftest` in action, from the cloned `dperftest` directory, simply run:

```
./Release/gen_simple_data -n 100000 --output queries.txt --output_golden goldens.txt
```

Then:

```
./Release/example_simple_service
```

And finally, from the same directory, in a different terminal, while the example server:

```
dperftest --url localhost:3000 --queries queries.txt --goldens goldens.txt 
```
