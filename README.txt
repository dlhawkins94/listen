================================================================================
Regarding systems ("systms")
================================================================================

The vowel-less name is because I'm foolishly using namespace std, and somewhere
in the standard libraries the name "system" is taken. I wanted a quick fix.

The systm type is defined in the audiotk library; in short, it's an abstract
type which expects the function "apply", which takes and returns a bus. A bus
is just a typedef of vector<frame>; a frame is mostly just a struct containing
a MatrixXcd.

The ASR code implements many systems which inherit the systm type. Generally,
if it has a state or requires a large number of funky parameters, a transform
or operation will be packed into a subclass of systm. This allows for conformity
and consistency throughout my code; a sequence of operations might look like
this:

bus b = in.apply({});
b = fft.apply(b);
b = foobar.apply(b);
scope1.apply({b[1]});
scope2.apply({b[2]});

Because the bus type is just a vector, it's easy to assemble and pick apart
buses using initializer lists. Sometimes this code gets awkward, but it's the
simplest way to get multiple inputs and outputs without rewriting Simulink.

Additionally, take to make parameter lists easier to work with when
initializing, system parameters which don't change during runtime are shoved
into a single struct, so if you have some system foo, all its parameters go into
a struct foo_params. foo then has a constructor foo::foo(foo_params params);
it's very easy and elegant to use initializer lists w/ these constructors.

Systems which have some special parameters which need to be trained and saved
have a constructor which takes an ifstream, and a save function which takes
an ofstream. For a system which contains other systems, the member systems are
read from the file first (using constructor initialization). The output is just
a binary blob.

================================================================================
Regarding multithreading
================================================================================

to prevent some std::eldritch_horror from spawning in
my code, I only add multithreading where it seems helpful and easy. The pattern
I'm using is something like this:

void some_system::func_helper(const Matrix &input0, const Matrix &input1,...
                              Matrix &output0, Matrix &output1, ...
			      mutex &mtx, atomic<int> &cnt) {
  int n = cnt++;
  while (n < input0.cols()) {
    Vector x = input0.col(n);
    ...
    mtx.lock();
    ... mutate the outputs ...
    mtx.unlock();
    n = cnt++;  
  }
}

To use:
th = thread(&some_system::func_helper, this, ref(x), ..., ref(mtx), ref(cnt));
th.join();

... creating as many threads as you desire. This pattern takes code that would
work on one vector at a time and parallelizes it.

Having the inputs be const should make them thread-safe (r-right?) That's
assuming no other threads have access to the input matrices.
The counter is atomic; when it exceeds the size of the main input matrix each
thread will join. 
