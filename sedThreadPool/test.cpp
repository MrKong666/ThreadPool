#include"threadpool.h"
#include<chrono>

class A :public Task{
public:
	A(int a,int b):l(a),r(b){}
	int l, r;
	Any run() {
		int sum = 0;
		for (int i = l; i <=r; i++) {
			sum += i;
		}
	 this_thread::sleep_for(chrono::seconds(3));
		return sum;
	}
	
};
int main() {
	ThreadPool pool;
//pool.setMode(PoolMode::MODE_CACHED);
pool.start(4);
Result r1=	pool.submitTask(make_shared<A>(1,100));
Result r2 = pool.submitTask(make_shared<A>(101,200));
Result r3 = pool.submitTask(make_shared<A>(201,300));
Result r4 = pool.submitTask(make_shared<A>(1, 100));
Result r5 = pool.submitTask(make_shared<A>(1, 100));
Result r6 = pool.submitTask(make_shared<A>(1, 100));
Result r8 = pool.submitTask(make_shared<A>(1, 100));
 
//int a=	r1.get().cast_<int>();
//int b = r2.get().cast_<int>();
//int c = r3.get().cast_<int>();
//cout << a << endl;
//cout << b << endl;
//cout << c << endl;
	return 0;
}