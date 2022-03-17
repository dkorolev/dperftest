package myapi

setcontains(xs, x) { xs[_] = x }

default allow = false
allow {
  setcontains(input.allowed[input.resource], input.role)
}
