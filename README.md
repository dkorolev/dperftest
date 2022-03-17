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

The `make install` command copies the freshly built `dperftest` binary into `/usr/local/bin`.

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

## Example with OPA

Similar to the section above, an example that mimics OPA can be run. After `cd dperftest`, run:

```
# Generate test queries and run the test.
./Release/example_opa_gen -n 50000 > opa_queries.txt
./Release/example_opa_stub

# And then, from a separate terminal:
dperftest --url http://localhost:8181/v1/data/myapi/allow --queries opa_queries.txt
```

The difference between this example and the one above is that instead of running the "stub", an OPA server can be run and benchmarked. To do so, once you have `opa` installed, execute the following two commands (the second one in a separate terminal):

```
# Important to add `-l error`, otherwise every OPA query
# will result in a line printed to the terminal, hurting QPS 2x.
opa run --server -l error

# Upload the policy into the running OPA instance.
# Run this command a few seconds after the previous one, to be sure.
# The expected response is `{}`.
curl -s -X PUT http://localhost:8181/v1/policies/myapi --data-binary @example_opa/policy.rego
```

After the OPA server is running, perftest it using the above `dperftest` command, with the same generated test data. You can also use `--write_goldens opa_goldens.txt` to save the results, and then confirm, with `--goldens opa_goldens.txt`, that the stub implementation and the real OPA server return the same results.
