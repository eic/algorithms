### `algorithms`: a Summary

This code defines a framework for creating and managing algorithms in a C++ application. Here's a breakdown of its key components:

**1. Input/Output Type Handling:**

* `Input<T...>` and `Output<T...>`: These template structs define how algorithm inputs and outputs are handled. They use `input_type_t` and `output_type_t` to map different types (`T`, `std::optional<T>`, `std::vector<T>`, `std::vector<std::optional<T>>`) to appropriate pointer types, ensuring non-null inputs and allowing for optional or vector-based arguments.
  * `T` → `gsl::not_null<const T*>`: a required argument.
  * `std::optional<T>` → `const T*`: an optional argument, `nullptr` when absent.
  * `std::vector<T>` → `std::vector<gsl::not_null<const T*>>`: a variadic argument with N required entries (N may be zero).
  * `std::vector<std::optional<T>>` → `std::vector<const T*>`: a variadic argument with N optional entries, each `nullptr` when absent, in the same order as the names.

  Outputs follow the same scheme with `T*` in place of `const T*`.

**2. Algorithm Base Classes:**

* `AlgorithmBase`: A base class for all algorithms, providing common features like properties, logging, and naming.
* `Algorithm<InputType, OutputType>`: A template class defining the interface for concrete algorithms. It includes methods for initialization (`init`), processing (`process`), and accessing input/output names.

**3. Algorithm Service (`AlgorithmSvc`):**

* Stores factories for creating algorithm instances.
* Allows adding new algorithm factories (`add`).
* Provides methods for retrieving algorithm instances (`get`) and listing available algorithms (`ls`).

**4. Utility Classes and Traits:**

* `Error`: A base class for exceptions within the framework.
* `GeoSvc`: A service for accessing geometry information (likely related to DD4hep).
* `LogSvc`: A logging service with configurable log levels and actions.
* `NameMixin`: Provides a consistent API for managing algorithm names and descriptions.
* `Configurable` and `PropertyMixin`: Enable algorithms to have configurable properties.
* `RandomSvc`: A service for generating random numbers using various distributions.
* `Service<SvcType>` and `ServiceSvc`: Infrastructure for managing services as lazy-evaluated singletons.
* Type traits (`is_vector`, `is_optional`, `is_vector_of_optional`, `data_type_t`, `input_type_t`, `output_type_t`): Help with argument type deduction and mapping.

**In essence, this code provides a structured way to:**

* Define algorithms with well-defined inputs and outputs.
* Manage algorithm creation and access through a service.
* Configure algorithms using properties.
* Incorporate logging and error handling.
* Utilize common services like geometry and random number generation.

This framework is designed to promote modularity, reusability, and maintainability of algorithms within a larger C++ application.

### Testing

Tests use Catch2 v3 and are built with `BUILD_TESTING=ON` (the default). Run them with:

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

The address, undefined behavior and thread sanitizers are enabled with `-DUSE_ASAN=ON`, `-DUSE_UBSAN=ON` and `-DUSE_TSAN=ON` (ASan and UBSan can be combined; TSan cannot be combined with ASan). Use the same runtime options as CI, for example:

```bash
export LSAN_OPTIONS=suppressions=$PWD/.github/lsan.supp
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export TSAN_OPTIONS=halt_on_error=1
```

### Documentation

The API reference is generated with [Doxygen](https://www.doxygen.nl) and published at <https://eic.github.io/algorithms/>. To build it locally (requires `doxygen` and `graphviz`):

```bash
cmake -B build -S . -DGENERATE_DOCS=ON
cmake --build build --target docs
```

The HTML output is written to `build/docs/html/`. Alternatively, run `doxygen build/Doxyfile` from the source directory after configuring.
