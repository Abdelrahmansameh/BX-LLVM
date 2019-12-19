@num_fibs = global i64 30, align 8 
define void @main() {
L0:
	 %x0 = add nsw i64 0,0
	 %x1 = add nsw i64 0,0
	 %x2 = load i64, i64* @num_fibs, align 8
	 call void  @fib(i64 %x2)
	 br label %L1
L1:
	 ret void
}
define void @fib(i64 %x7) {
L4:
	 %x3 = add nsw i64 0,0
	 %x4 = add nsw i64 0,0
	 %x5 = add nsw i64 0,0
	 %x6 = add nsw i64 0,1
	 call void  @fib_aux(i64 %x5, i64 %x6, i64 %x7)
	 br label %L5
L5:
	 ret void
}
define void @fib_aux(i64 %x18, i64 %x23, i64 %x13) {
L9:
	 %x8 = add nsw i64 0,0
	 %x9 = add nsw i64 0,0
	 %x10 = add nsw i64 0,0
	 %x11 = add nsw i64 0,0
	 %x12 = add nsw i64 0,1
	 %x14 = icmp slt i64 %x13, %x12
	 br i1 %x14, label %L13, label %L12
L13:
	 br label %L10
L12:
	 br label %L14
L10:
	 %x16 = phi i64 [ %x15, %L14], [ %x8, %L13] 
	 %x19 = phi i64 [ %x17, %L14], [ %x18, %L13] 
	 %x21 = phi i64 [ %x20, %L14], [ %x13, %L13] 
	 %x24 = phi i64 [ %x22, %L14], [ %x23, %L13] 
	 %x26 = phi i64 [ %x25, %L14], [ %x10, %L13] 
	 %x28 = phi i64 [ %x27, %L14], [ %x11, %L13] 
	 ret void
L14:
	 call void  @bx_print_int(i64 %x17)
	 %x29 = add nsw i64 %x17, 0
	 %x15 = add nsw i64 %x22, %x29
	 %x30 = add nsw i64 %x20, 0
	 %x27 = add nsw i64 0,1
	 %x25 = sub nsw i64 %x27, %x30
	 call void  @fib_aux(i64 %x22, i64 %x15, i64 %x25)
	 br label %L10
}
