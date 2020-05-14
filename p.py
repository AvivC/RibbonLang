class B:
	def f(self):
		print("f")

class C(B):
	def f(self):
		print(type(super()))

C().f()