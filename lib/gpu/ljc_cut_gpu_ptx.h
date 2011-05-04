const char * ljc_cut_gpu_kernel = 
"	.version 1.4\n"
"	.target sm_13\n"
"	.tex .u64 pos_tex;\n"
"	.tex .u64 q_tex;\n"
"	.entry kernel_pair (\n"
"		.param .u64 __cudaparm_kernel_pair_x_,\n"
"		.param .u64 __cudaparm_kernel_pair_lj1,\n"
"		.param .u64 __cudaparm_kernel_pair_lj3,\n"
"		.param .s32 __cudaparm_kernel_pair_lj_types,\n"
"		.param .u64 __cudaparm_kernel_pair_sp_lj_in,\n"
"		.param .u64 __cudaparm_kernel_pair_dev_nbor,\n"
"		.param .u64 __cudaparm_kernel_pair_ans,\n"
"		.param .u64 __cudaparm_kernel_pair_engv,\n"
"		.param .s32 __cudaparm_kernel_pair_eflag,\n"
"		.param .s32 __cudaparm_kernel_pair_vflag,\n"
"		.param .s32 __cudaparm_kernel_pair_inum,\n"
"		.param .s32 __cudaparm_kernel_pair_nall,\n"
"		.param .s32 __cudaparm_kernel_pair_nbor_pitch,\n"
"		.param .u64 __cudaparm_kernel_pair_q_,\n"
"		.param .u64 __cudaparm_kernel_pair_cutsq,\n"
"		.param .f32 __cudaparm_kernel_pair_qqrd2e)\n"
"	{\n"
"	.reg .u32 %r<42>;\n"
"	.reg .u64 %rd<39>;\n"
"	.reg .f32 %f<113>;\n"
"	.reg .pred %p<10>;\n"
"	.shared .align 4 .b8 __cuda_sp_lj108[32];\n"
"	.loc	14	99	0\n"
"$LBB1_kernel_pair:\n"
"	.loc	14	103	0\n"
"	ld.param.u64 	%rd1, [__cudaparm_kernel_pair_sp_lj_in];\n"
"	ld.global.f32 	%f1, [%rd1+0];\n"
"	st.shared.f32 	[__cuda_sp_lj108+0], %f1;\n"
"	.loc	14	104	0\n"
"	ld.global.f32 	%f2, [%rd1+4];\n"
"	st.shared.f32 	[__cuda_sp_lj108+4], %f2;\n"
"	.loc	14	105	0\n"
"	ld.global.f32 	%f3, [%rd1+8];\n"
"	st.shared.f32 	[__cuda_sp_lj108+8], %f3;\n"
"	.loc	14	106	0\n"
"	ld.global.f32 	%f4, [%rd1+12];\n"
"	st.shared.f32 	[__cuda_sp_lj108+12], %f4;\n"
"	.loc	14	107	0\n"
"	ld.global.f32 	%f5, [%rd1+16];\n"
"	st.shared.f32 	[__cuda_sp_lj108+16], %f5;\n"
"	.loc	14	108	0\n"
"	ld.global.f32 	%f6, [%rd1+20];\n"
"	st.shared.f32 	[__cuda_sp_lj108+20], %f6;\n"
"	.loc	14	109	0\n"
"	ld.global.f32 	%f7, [%rd1+24];\n"
"	st.shared.f32 	[__cuda_sp_lj108+24], %f7;\n"
"	.loc	14	110	0\n"
"	ld.global.f32 	%f8, [%rd1+28];\n"
"	st.shared.f32 	[__cuda_sp_lj108+28], %f8;\n"
"	cvt.s32.u16 	%r1, %ctaid.x;\n"
"	cvt.s32.u16 	%r2, %ntid.x;\n"
"	mul24.lo.s32 	%r3, %r1, %r2;\n"
"	cvt.u32.u16 	%r4, %tid.x;\n"
"	add.u32 	%r5, %r3, %r4;\n"
"	ld.param.s32 	%r6, [__cudaparm_kernel_pair_inum];\n"
"	setp.le.s32 	%p1, %r6, %r5;\n"
"	@%p1 bra 	$Lt_0_10242;\n"
"	.loc	14	121	0\n"
"	mov.f32 	%f9, 0f00000000;     	\n"
"	mov.f32 	%f10, %f9;\n"
"	mov.f32 	%f11, 0f00000000;    	\n"
"	mov.f32 	%f12, %f11;\n"
"	mov.f32 	%f13, 0f00000000;    	\n"
"	mov.f32 	%f14, %f13;\n"
"	mov.f32 	%f15, 0f00000000;    	\n"
"	mov.f32 	%f16, %f15;\n"
"	mov.f32 	%f17, 0f00000000;    	\n"
"	mov.f32 	%f18, %f17;\n"
"	mov.f32 	%f19, 0f00000000;    	\n"
"	mov.f32 	%f20, %f19;\n"
"	.loc	14	124	0\n"
"	cvt.u64.s32 	%rd2, %r5;\n"
"	mul.lo.u64 	%rd3, %rd2, 4;\n"
"	ld.param.u64 	%rd4, [__cudaparm_kernel_pair_dev_nbor];\n"
"	add.u64 	%rd5, %rd4, %rd3;\n"
"	ld.global.s32 	%r7, [%rd5+0];\n"
"	.loc	14	126	0\n"
"	ld.param.s32 	%r8, [__cudaparm_kernel_pair_nbor_pitch];\n"
"	cvt.u64.s32 	%rd6, %r8;\n"
"	mul.lo.u64 	%rd7, %rd6, 4;\n"
"	add.u64 	%rd8, %rd5, %rd7;\n"
"	ld.global.s32 	%r9, [%rd8+0];\n"
"	.loc	14	127	0\n"
"	add.u64 	%rd9, %rd8, %rd7;\n"
"	mov.s64 	%rd10, %rd9;\n"
"	mov.s32 	%r10, %r7;\n"
"	mov.s32 	%r11, 0;\n"
"	mov.s32 	%r12, 0;\n"
"	mov.s32 	%r13, 0;\n"
"	tex.1d.v4.f32.s32 {%f21,%f22,%f23,%f24},[pos_tex,{%r10,%r11,%r12,%r13}];\n"
"	.loc	14	130	0\n"
"	mov.f32 	%f25, %f21;\n"
"	mov.f32 	%f26, %f22;\n"
"	mov.f32 	%f27, %f23;\n"
"	mov.f32 	%f28, %f24;\n"
"	mov.s32 	%r14, %r7;\n"
"	mov.s32 	%r15, 0;\n"
"	mov.s32 	%r16, 0;\n"
"	mov.s32 	%r17, 0;\n"
"	tex.1d.v4.f32.s32 {%f29,%f30,%f31,%f32},[q_tex,{%r14,%r15,%r16,%r17}];\n"
"	.loc	14	131	0\n"
"	mov.f32 	%f33, %f29;\n"
"	mul24.lo.s32 	%r18, %r9, %r8;\n"
"	cvt.s64.s32 	%rd11, %r18;\n"
"	mul.lo.u64 	%rd12, %rd11, 4;\n"
"	add.u64 	%rd13, %rd9, %rd12;\n"
"	ld.param.s32 	%r19, [__cudaparm_kernel_pair_vflag];\n"
"	ld.param.s32 	%r20, [__cudaparm_kernel_pair_eflag];\n"
"	setp.ge.u64 	%p2, %rd9, %rd13;\n"
"	mov.f32 	%f34, 0f00000000;    	\n"
"	mov.f32 	%f35, 0f00000000;    	\n"
"	mov.f32 	%f36, 0f00000000;    	\n"
"	mov.f32 	%f37, 0f00000000;    	\n"
"	mov.f32 	%f38, 0f00000000;    	\n"
"	@%p2 bra 	$Lt_0_15874;\n"
"	mov.s32 	%r21, 0;\n"
"	setp.gt.s32 	%p3, %r20, %r21;\n"
"	mov.s32 	%r22, 0;\n"
"	setp.gt.s32 	%p4, %r19, %r22;\n"
"	cvt.rzi.s32.f32 	%r23, %f28;\n"
"	ld.param.s32 	%r24, [__cudaparm_kernel_pair_lj_types];\n"
"	mul.lo.s32 	%r25, %r24, %r23;\n"
"	ld.param.u64 	%rd14, [__cudaparm_kernel_pair_cutsq];\n"
"	mov.u64 	%rd15, __cuda_sp_lj108;\n"
"$Lt_0_11266:\n"
"	.loc	14	135	0\n"
"	ld.global.s32 	%r26, [%rd10+0];\n"
"	.loc	14	138	0\n"
"	shr.s32 	%r27, %r26, 30;\n"
"	cvt.s64.s32 	%rd16, %r27;\n"
"	and.b64 	%rd17, %rd16, 3;\n"
"	mul.lo.u64 	%rd18, %rd17, 4;\n"
"	add.u64 	%rd19, %rd15, %rd18;\n"
"	ld.shared.f32 	%f39, [%rd19+0];\n"
"	.loc	14	139	0\n"
"	ld.shared.f32 	%f40, [%rd19+16];\n"
"	and.b32 	%r28, %r26, 1073741823;\n"
"	mov.s32 	%r29, %r28;\n"
"	mov.s32 	%r30, 0;\n"
"	mov.s32 	%r31, 0;\n"
"	mov.s32 	%r32, 0;\n"
"	tex.1d.v4.f32.s32 {%f41,%f42,%f43,%f44},[pos_tex,{%r29,%r30,%r31,%r32}];\n"
"	.loc	14	142	0\n"
"	mov.f32 	%f45, %f41;\n"
"	mov.f32 	%f46, %f42;\n"
"	mov.f32 	%f47, %f43;\n"
"	mov.f32 	%f48, %f44;\n"
"	cvt.rzi.s32.f32 	%r33, %f48;\n"
"	sub.f32 	%f49, %f26, %f46;\n"
"	sub.f32 	%f50, %f25, %f45;\n"
"	sub.f32 	%f51, %f27, %f47;\n"
"	mul.f32 	%f52, %f49, %f49;\n"
"	mad.f32 	%f53, %f50, %f50, %f52;\n"
"	add.s32 	%r34, %r33, %r25;\n"
"	cvt.u64.s32 	%rd20, %r34;\n"
"	mad.f32 	%f54, %f51, %f51, %f53;\n"
"	mul.lo.u64 	%rd21, %rd20, 4;\n"
"	add.u64 	%rd22, %rd14, %rd21;\n"
"	ld.global.f32 	%f55, [%rd22+0];\n"
"	setp.gt.f32 	%p5, %f55, %f54;\n"
"	@!%p5 bra 	$Lt_0_14082;\n"
"	mul.lo.u64 	%rd23, %rd20, 16;\n"
"	rcp.approx.f32 	%f56, %f54;\n"
"	ld.param.u64 	%rd24, [__cudaparm_kernel_pair_lj1];\n"
"	add.u64 	%rd25, %rd24, %rd23;\n"
"	ld.global.f32 	%f57, [%rd25+8];\n"
"	setp.lt.f32 	%p6, %f54, %f57;\n"
"	@!%p6 bra 	$Lt_0_12290;\n"
"	.loc	14	157	0\n"
"	mul.f32 	%f58, %f56, %f56;\n"
"	mul.f32 	%f59, %f56, %f58;\n"
"	mov.f32 	%f60, %f59;\n"
"	.loc	14	138	0\n"
"	ld.shared.f32 	%f39, [%rd19+0];\n"
"	.loc	14	158	0\n"
"	mul.f32 	%f61, %f59, %f39;\n"
"	ld.global.v2.f32 	{%f62,%f63}, [%rd25+0];\n"
"	mul.f32 	%f64, %f62, %f59;\n"
"	sub.f32 	%f65, %f64, %f63;\n"
"	mul.f32 	%f66, %f61, %f65;\n"
"	bra.uni 	$Lt_0_12034;\n"
"$Lt_0_12290:\n"
"	.loc	14	160	0\n"
"	mov.f32 	%f66, 0f00000000;    	\n"
"$Lt_0_12034:\n"
"	ld.global.f32 	%f67, [%rd25+12];\n"
"	setp.gt.f32 	%p7, %f67, %f54;\n"
"	@!%p7 bra 	$Lt_0_12802;\n"
"	mov.s32 	%r35, %r28;\n"
"	mov.s32 	%r36, 0;\n"
"	mov.s32 	%r37, 0;\n"
"	mov.s32 	%r38, 0;\n"
"	tex.1d.v4.f32.s32 {%f68,%f69,%f70,%f71},[q_tex,{%r35,%r36,%r37,%r38}];\n"
"	.loc	14	163	0\n"
"	mov.f32 	%f72, %f68;\n"
"	ld.param.f32 	%f73, [__cudaparm_kernel_pair_qqrd2e];\n"
"	mul.f32 	%f74, %f73, %f33;\n"
"	mul.f32 	%f75, %f72, %f74;\n"
"	sqrt.approx.f32 	%f76, %f56;\n"
"	mul.f32 	%f77, %f75, %f76;\n"
"	.loc	14	139	0\n"
"	ld.shared.f32 	%f40, [%rd19+16];\n"
"	.loc	14	163	0\n"
"	mul.f32 	%f78, %f40, %f77;\n"
"	bra.uni 	$Lt_0_12546;\n"
"$Lt_0_12802:\n"
"	.loc	14	165	0\n"
"	mov.f32 	%f78, 0f00000000;    	\n"
"$Lt_0_12546:\n"
"	.loc	14	169	0\n"
"	add.f32 	%f79, %f78, %f66;\n"
"	mul.f32 	%f80, %f79, %f56;\n"
"	mad.f32 	%f36, %f50, %f80, %f36;\n"
"	.loc	14	170	0\n"
"	mad.f32 	%f35, %f49, %f80, %f35;\n"
"	.loc	14	171	0\n"
"	mad.f32 	%f34, %f51, %f80, %f34;\n"
"	@!%p3 bra 	$Lt_0_13570;\n"
"	.loc	14	174	0\n"
"	add.f32 	%f37, %f78, %f37;\n"
"	@!%p6 bra 	$Lt_0_13570;\n"
"	.loc	14	177	0\n"
"	ld.param.u64 	%rd26, [__cudaparm_kernel_pair_lj3];\n"
"	add.u64 	%rd27, %rd26, %rd23;\n"
"	mov.f32 	%f81, %f60;\n"
"	ld.global.v4.f32 	{%f82,%f83,%f84,_}, [%rd27+0];\n"
"	mul.f32 	%f85, %f82, %f81;\n"
"	sub.f32 	%f86, %f85, %f83;\n"
"	mul.f32 	%f87, %f81, %f86;\n"
"	sub.f32 	%f88, %f87, %f84;\n"
"	.loc	14	138	0\n"
"	ld.shared.f32 	%f39, [%rd19+0];\n"
"	.loc	14	177	0\n"
"	mad.f32 	%f38, %f39, %f88, %f38;\n"
"$Lt_0_13570:\n"
"$Lt_0_13058:\n"
"	@!%p4 bra 	$Lt_0_14082;\n"
"	.loc	14	181	0\n"
"	mov.f32 	%f89, %f10;\n"
"	mul.f32 	%f90, %f50, %f50;\n"
"	mad.f32 	%f91, %f80, %f90, %f89;\n"
"	mov.f32 	%f10, %f91;\n"
"	.loc	14	182	0\n"
"	mov.f32 	%f92, %f12;\n"
"	mad.f32 	%f93, %f80, %f52, %f92;\n"
"	mov.f32 	%f12, %f93;\n"
"	.loc	14	183	0\n"
"	mov.f32 	%f94, %f14;\n"
"	mul.f32 	%f95, %f51, %f51;\n"
"	mad.f32 	%f96, %f80, %f95, %f94;\n"
"	mov.f32 	%f14, %f96;\n"
"	.loc	14	184	0\n"
"	mov.f32 	%f97, %f16;\n"
"	mul.f32 	%f98, %f49, %f50;\n"
"	mad.f32 	%f99, %f80, %f98, %f97;\n"
"	mov.f32 	%f16, %f99;\n"
"	.loc	14	185	0\n"
"	mov.f32 	%f100, %f18;\n"
"	mul.f32 	%f101, %f50, %f51;\n"
"	mad.f32 	%f102, %f80, %f101, %f100;\n"
"	mov.f32 	%f18, %f102;\n"
"	.loc	14	186	0\n"
"	mul.f32 	%f103, %f49, %f51;\n"
"	mad.f32 	%f19, %f80, %f103, %f19;\n"
"	mov.f32 	%f104, %f19;\n"
"$Lt_0_14082:\n"
"$Lt_0_11522:\n"
"	.loc	14	134	0\n"
"	add.u64 	%rd10, %rd7, %rd10;\n"
"	setp.gt.u64 	%p8, %rd13, %rd10;\n"
"	@%p8 bra 	$Lt_0_11266;\n"
"	bra.uni 	$Lt_0_10754;\n"
"$Lt_0_15874:\n"
"	mov.s32 	%r39, 0;\n"
"	setp.gt.s32 	%p3, %r20, %r39;\n"
"	mov.s32 	%r40, 0;\n"
"	setp.gt.s32 	%p4, %r19, %r40;\n"
"$Lt_0_10754:\n"
"	.loc	14	193	0\n"
"	ld.param.u64 	%rd28, [__cudaparm_kernel_pair_engv];\n"
"	add.u64 	%rd29, %rd28, %rd3;\n"
"	@!%p3 bra 	$Lt_0_14850;\n"
"	.loc	14	195	0\n"
"	st.global.f32 	[%rd29+0], %f38;\n"
"	.loc	14	196	0\n"
"	cvt.u64.s32 	%rd30, %r6;\n"
"	mul.lo.u64 	%rd31, %rd30, 4;\n"
"	add.u64 	%rd29, %rd31, %rd29;\n"
"	.loc	14	197	0\n"
"	st.global.f32 	[%rd29+0], %f37;\n"
"	.loc	14	198	0\n"
"	add.u64 	%rd29, %rd31, %rd29;\n"
"$Lt_0_14850:\n"
"	@!%p4 bra 	$Lt_0_15362;\n"
"	.loc	14	202	0\n"
"	mov.f32 	%f105, %f10;\n"
"	st.global.f32 	[%rd29+0], %f105;\n"
"	.loc	14	203	0\n"
"	cvt.u64.s32 	%rd32, %r6;\n"
"	mul.lo.u64 	%rd33, %rd32, 4;\n"
"	add.u64 	%rd29, %rd33, %rd29;\n"
"	.loc	14	202	0\n"
"	mov.f32 	%f106, %f12;\n"
"	st.global.f32 	[%rd29+0], %f106;\n"
"	.loc	14	203	0\n"
"	add.u64 	%rd29, %rd33, %rd29;\n"
"	.loc	14	202	0\n"
"	mov.f32 	%f107, %f14;\n"
"	st.global.f32 	[%rd29+0], %f107;\n"
"	.loc	14	203	0\n"
"	add.u64 	%rd29, %rd33, %rd29;\n"
"	.loc	14	202	0\n"
"	mov.f32 	%f108, %f16;\n"
"	st.global.f32 	[%rd29+0], %f108;\n"
"	.loc	14	203	0\n"
"	add.u64 	%rd29, %rd33, %rd29;\n"
"	.loc	14	202	0\n"
"	mov.f32 	%f109, %f18;\n"
"	st.global.f32 	[%rd29+0], %f109;\n"
"	add.u64 	%rd34, %rd33, %rd29;\n"
"	st.global.f32 	[%rd34+0], %f19;\n"
"$Lt_0_15362:\n"
"	.loc	14	206	0\n"
"	ld.param.u64 	%rd35, [__cudaparm_kernel_pair_ans];\n"
"	mul.lo.u64 	%rd36, %rd2, 16;\n"
"	add.u64 	%rd37, %rd35, %rd36;\n"
"	mov.f32 	%f110, %f111;\n"
"	st.global.v4.f32 	[%rd37+0], {%f36,%f35,%f34,%f110};\n"
"$Lt_0_10242:\n"
"	.loc	14	208	0\n"
"	exit;\n"
"$LDWend_kernel_pair:\n"
"	}\n"
"	.entry kernel_pair_fast (\n"
"		.param .u64 __cudaparm_kernel_pair_fast_x_,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_lj1_in,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_lj3_in,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_sp_lj_in,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_dev_nbor,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_ans,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_engv,\n"
"		.param .s32 __cudaparm_kernel_pair_fast_eflag,\n"
"		.param .s32 __cudaparm_kernel_pair_fast_vflag,\n"
"		.param .s32 __cudaparm_kernel_pair_fast_inum,\n"
"		.param .s32 __cudaparm_kernel_pair_fast_nall,\n"
"		.param .s32 __cudaparm_kernel_pair_fast_nbor_pitch,\n"
"		.param .u64 __cudaparm_kernel_pair_fast_q_,\n"
"		.param .u64 __cudaparm_kernel_pair_fast__cutsq,\n"
"		.param .f32 __cudaparm_kernel_pair_fast_qqrd2e)\n"
"	{\n"
"	.reg .u32 %r<45>;\n"
"	.reg .u64 %rd<55>;\n"
"	.reg .f32 %f<117>;\n"
"	.reg .pred %p<13>;\n"
"	.shared .align 4 .b8 __cuda_sp_lj244[32];\n"
"	.shared .align 16 .b8 __cuda_lj1288[1024];\n"
"	.shared .align 4 .b8 __cuda_cutsq1312[256];\n"
"	.shared .align 16 .b8 __cuda_lj31568[1024];\n"
"	.loc	14	217	0\n"
"$LBB1_kernel_pair_fast:\n"
"	cvt.s32.u16 	%r1, %tid.x;\n"
"	mov.u32 	%r2, 7;\n"
"	setp.gt.s32 	%p1, %r1, %r2;\n"
"	@%p1 bra 	$Lt_1_12546;\n"
"	.loc	14	225	0\n"
"	mov.u64 	%rd1, __cuda_sp_lj244;\n"
"	cvt.u64.s32 	%rd2, %r1;\n"
"	mul.lo.u64 	%rd3, %rd2, 4;\n"
"	ld.param.u64 	%rd4, [__cudaparm_kernel_pair_fast_sp_lj_in];\n"
"	add.u64 	%rd5, %rd4, %rd3;\n"
"	ld.global.f32 	%f1, [%rd5+0];\n"
"	add.u64 	%rd6, %rd3, %rd1;\n"
"	st.shared.f32 	[%rd6+0], %f1;\n"
"$Lt_1_12546:\n"
"	mov.u64 	%rd1, __cuda_sp_lj244;\n"
"	mov.u32 	%r3, 63;\n"
"	setp.gt.s32 	%p2, %r1, %r3;\n"
"	@%p2 bra 	$Lt_1_13058;\n"
"	.loc	14	227	0\n"
"	mov.u64 	%rd7, __cuda_lj1288;\n"
"	mov.u64 	%rd8, __cuda_cutsq1312;\n"
"	cvt.u64.s32 	%rd9, %r1;\n"
"	mul.lo.u64 	%rd10, %rd9, 16;\n"
"	ld.param.u64 	%rd11, [__cudaparm_kernel_pair_fast_lj1_in];\n"
"	add.u64 	%rd12, %rd11, %rd10;\n"
"	add.u64 	%rd13, %rd10, %rd7;\n"
"	ld.global.v4.f32 	{%f2,%f3,%f4,%f5}, [%rd12+0];\n"
"	st.shared.f32 	[%rd13+0], %f2;\n"
"	st.shared.f32 	[%rd13+4], %f3;\n"
"	st.shared.f32 	[%rd13+8], %f4;\n"
"	st.shared.f32 	[%rd13+12], %f5;\n"
"	.loc	14	228	0\n"
"	mul.lo.u64 	%rd14, %rd9, 4;\n"
"	ld.param.u64 	%rd15, [__cudaparm_kernel_pair_fast__cutsq];\n"
"	add.u64 	%rd16, %rd15, %rd14;\n"
"	ld.global.f32 	%f6, [%rd16+0];\n"
"	add.u64 	%rd17, %rd14, %rd8;\n"
"	st.shared.f32 	[%rd17+0], %f6;\n"
"	ld.param.s32 	%r4, [__cudaparm_kernel_pair_fast_eflag];\n"
"	mov.u32 	%r5, 0;\n"
"	setp.le.s32 	%p3, %r4, %r5;\n"
"	@%p3 bra 	$Lt_1_13570;\n"
"	.loc	14	230	0\n"
"	mov.u64 	%rd18, __cuda_lj31568;\n"
"	ld.param.u64 	%rd19, [__cudaparm_kernel_pair_fast_lj3_in];\n"
"	add.u64 	%rd20, %rd19, %rd10;\n"
"	add.u64 	%rd21, %rd10, %rd18;\n"
"	ld.global.v4.f32 	{%f7,%f8,%f9,%f10}, [%rd20+0];\n"
"	st.shared.f32 	[%rd21+0], %f7;\n"
"	st.shared.f32 	[%rd21+4], %f8;\n"
"	st.shared.f32 	[%rd21+8], %f9;\n"
"	st.shared.f32 	[%rd21+12], %f10;\n"
"$Lt_1_13570:\n"
"	mov.u64 	%rd18, __cuda_lj31568;\n"
"$Lt_1_13058:\n"
"	mov.u64 	%rd7, __cuda_lj1288;\n"
"	mov.u64 	%rd8, __cuda_cutsq1312;\n"
"	mov.u64 	%rd18, __cuda_lj31568;\n"
"	.loc	14	233	0\n"
"	bar.sync 	0;\n"
"	cvt.s32.u16 	%r6, %ctaid.x;\n"
"	cvt.s32.u16 	%r7, %ntid.x;\n"
"	mul24.lo.s32 	%r8, %r6, %r7;\n"
"	add.s32 	%r9, %r8, %r1;\n"
"	ld.param.s32 	%r10, [__cudaparm_kernel_pair_fast_inum];\n"
"	setp.ge.s32 	%p4, %r9, %r10;\n"
"	@%p4 bra 	$Lt_1_14082;\n"
"	.loc	14	245	0\n"
"	mov.f32 	%f11, 0f00000000;    	\n"
"	mov.f32 	%f12, %f11;\n"
"	mov.f32 	%f13, 0f00000000;    	\n"
"	mov.f32 	%f14, %f13;\n"
"	mov.f32 	%f15, 0f00000000;    	\n"
"	mov.f32 	%f16, %f15;\n"
"	mov.f32 	%f17, 0f00000000;    	\n"
"	mov.f32 	%f18, %f17;\n"
"	mov.f32 	%f19, 0f00000000;    	\n"
"	mov.f32 	%f20, %f19;\n"
"	mov.f32 	%f21, 0f00000000;    	\n"
"	mov.f32 	%f22, %f21;\n"
"	.loc	14	248	0\n"
"	cvt.u64.s32 	%rd22, %r9;\n"
"	mul.lo.u64 	%rd23, %rd22, 4;\n"
"	ld.param.u64 	%rd24, [__cudaparm_kernel_pair_fast_dev_nbor];\n"
"	add.u64 	%rd25, %rd24, %rd23;\n"
"	ld.global.s32 	%r11, [%rd25+0];\n"
"	.loc	14	250	0\n"
"	ld.param.s32 	%r12, [__cudaparm_kernel_pair_fast_nbor_pitch];\n"
"	cvt.u64.s32 	%rd26, %r12;\n"
"	mul.lo.u64 	%rd27, %rd26, 4;\n"
"	add.u64 	%rd28, %rd25, %rd27;\n"
"	ld.global.s32 	%r13, [%rd28+0];\n"
"	.loc	14	251	0\n"
"	add.u64 	%rd29, %rd28, %rd27;\n"
"	mov.s64 	%rd30, %rd29;\n"
"	mov.s32 	%r14, %r11;\n"
"	mov.s32 	%r15, 0;\n"
"	mov.s32 	%r16, 0;\n"
"	mov.s32 	%r17, 0;\n"
"	tex.1d.v4.f32.s32 {%f23,%f24,%f25,%f26},[pos_tex,{%r14,%r15,%r16,%r17}];\n"
"	.loc	14	254	0\n"
"	mov.f32 	%f27, %f23;\n"
"	mov.f32 	%f28, %f24;\n"
"	mov.f32 	%f29, %f25;\n"
"	mov.f32 	%f30, %f26;\n"
"	mov.s32 	%r18, %r11;\n"
"	mov.s32 	%r19, 0;\n"
"	mov.s32 	%r20, 0;\n"
"	mov.s32 	%r21, 0;\n"
"	tex.1d.v4.f32.s32 {%f31,%f32,%f33,%f34},[q_tex,{%r18,%r19,%r20,%r21}];\n"
"	.loc	14	255	0\n"
"	mov.f32 	%f35, %f31;\n"
"	mul24.lo.s32 	%r22, %r13, %r12;\n"
"	cvt.s64.s32 	%rd31, %r22;\n"
"	mul.lo.u64 	%rd32, %rd31, 4;\n"
"	add.u64 	%rd33, %rd29, %rd32;\n"
"	ld.param.s32 	%r23, [__cudaparm_kernel_pair_fast_vflag];\n"
"	ld.param.s32 	%r24, [__cudaparm_kernel_pair_fast_eflag];\n"
"	setp.ge.u64 	%p5, %rd29, %rd33;\n"
"	mov.f32 	%f36, 0f00000000;    	\n"
"	mov.f32 	%f37, 0f00000000;    	\n"
"	mov.f32 	%f38, 0f00000000;    	\n"
"	mov.f32 	%f39, 0f00000000;    	\n"
"	mov.f32 	%f40, 0f00000000;    	\n"
"	@%p5 bra 	$Lt_1_19714;\n"
"	mov.s32 	%r25, 0;\n"
"	setp.gt.s32 	%p6, %r24, %r25;\n"
"	mov.s32 	%r26, 0;\n"
"	setp.gt.s32 	%p7, %r23, %r26;\n"
"	cvt.rzi.s32.f32 	%r27, %f30;\n"
"	mov.s32 	%r28, 8;\n"
"	mul24.lo.s32 	%r29, %r28, %r27;\n"
"	cvt.rn.f32.s32 	%f41, %r29;\n"
"$Lt_1_15106:\n"
"	.loc	14	260	0\n"
"	ld.global.s32 	%r30, [%rd30+0];\n"
"	.loc	14	263	0\n"
"	shr.s32 	%r31, %r30, 30;\n"
"	cvt.s64.s32 	%rd34, %r31;\n"
"	and.b64 	%rd35, %rd34, 3;\n"
"	mul.lo.u64 	%rd36, %rd35, 4;\n"
"	add.u64 	%rd37, %rd1, %rd36;\n"
"	ld.shared.f32 	%f42, [%rd37+0];\n"
"	.loc	14	264	0\n"
"	ld.shared.f32 	%f43, [%rd37+16];\n"
"	and.b32 	%r32, %r30, 1073741823;\n"
"	mov.s32 	%r33, %r32;\n"
"	mov.s32 	%r34, 0;\n"
"	mov.s32 	%r35, 0;\n"
"	mov.s32 	%r36, 0;\n"
"	tex.1d.v4.f32.s32 {%f44,%f45,%f46,%f47},[pos_tex,{%r33,%r34,%r35,%r36}];\n"
"	.loc	14	267	0\n"
"	mov.f32 	%f48, %f44;\n"
"	mov.f32 	%f49, %f45;\n"
"	mov.f32 	%f50, %f46;\n"
"	mov.f32 	%f51, %f47;\n"
"	sub.f32 	%f52, %f28, %f49;\n"
"	sub.f32 	%f53, %f27, %f48;\n"
"	sub.f32 	%f54, %f29, %f50;\n"
"	mul.f32 	%f55, %f52, %f52;\n"
"	mad.f32 	%f56, %f53, %f53, %f55;\n"
"	mad.f32 	%f57, %f54, %f54, %f56;\n"
"	add.f32 	%f58, %f41, %f51;\n"
"	cvt.rzi.s32.f32 	%r37, %f58;\n"
"	cvt.u64.s32 	%rd38, %r37;\n"
"	mul.lo.u64 	%rd39, %rd38, 4;\n"
"	add.u64 	%rd40, %rd8, %rd39;\n"
"	ld.shared.f32 	%f59, [%rd40+0];\n"
"	setp.gt.f32 	%p8, %f59, %f57;\n"
"	@!%p8 bra 	$Lt_1_17922;\n"
"	rcp.approx.f32 	%f60, %f57;\n"
"	mul.lo.u64 	%rd41, %rd38, 16;\n"
"	add.u64 	%rd42, %rd41, %rd7;\n"
"	ld.shared.f32 	%f61, [%rd42+8];\n"
"	setp.lt.f32 	%p9, %f57, %f61;\n"
"	@!%p9 bra 	$Lt_1_16130;\n"
"	.loc	14	281	0\n"
"	mul.f32 	%f62, %f60, %f60;\n"
"	mul.f32 	%f63, %f60, %f62;\n"
"	mov.f32 	%f64, %f63;\n"
"	.loc	14	263	0\n"
"	ld.shared.f32 	%f42, [%rd37+0];\n"
"	.loc	14	282	0\n"
"	mul.f32 	%f65, %f63, %f42;\n"
"	ld.shared.f32 	%f66, [%rd42+4];\n"
"	ld.shared.f32 	%f67, [%rd42+0];\n"
"	mul.f32 	%f68, %f67, %f63;\n"
"	sub.f32 	%f69, %f68, %f66;\n"
"	mul.f32 	%f70, %f65, %f69;\n"
"	bra.uni 	$Lt_1_15874;\n"
"$Lt_1_16130:\n"
"	.loc	14	284	0\n"
"	mov.f32 	%f70, 0f00000000;    	\n"
"$Lt_1_15874:\n"
"	ld.shared.f32 	%f71, [%rd42+12];\n"
"	setp.gt.f32 	%p10, %f71, %f57;\n"
"	@!%p10 bra 	$Lt_1_16642;\n"
"	mov.s32 	%r38, %r32;\n"
"	mov.s32 	%r39, 0;\n"
"	mov.s32 	%r40, 0;\n"
"	mov.s32 	%r41, 0;\n"
"	tex.1d.v4.f32.s32 {%f72,%f73,%f74,%f75},[q_tex,{%r38,%r39,%r40,%r41}];\n"
"	.loc	14	287	0\n"
"	mov.f32 	%f76, %f72;\n"
"	ld.param.f32 	%f77, [__cudaparm_kernel_pair_fast_qqrd2e];\n"
"	mul.f32 	%f78, %f77, %f35;\n"
"	mul.f32 	%f79, %f76, %f78;\n"
"	sqrt.approx.f32 	%f80, %f60;\n"
"	mul.f32 	%f81, %f79, %f80;\n"
"	.loc	14	264	0\n"
"	ld.shared.f32 	%f43, [%rd37+16];\n"
"	.loc	14	287	0\n"
"	mul.f32 	%f82, %f43, %f81;\n"
"	bra.uni 	$Lt_1_16386;\n"
"$Lt_1_16642:\n"
"	.loc	14	289	0\n"
"	mov.f32 	%f82, 0f00000000;    	\n"
"$Lt_1_16386:\n"
"	.loc	14	293	0\n"
"	add.f32 	%f83, %f82, %f70;\n"
"	mul.f32 	%f84, %f83, %f60;\n"
"	mad.f32 	%f38, %f53, %f84, %f38;\n"
"	.loc	14	294	0\n"
"	mad.f32 	%f37, %f52, %f84, %f37;\n"
"	.loc	14	295	0\n"
"	mad.f32 	%f36, %f54, %f84, %f36;\n"
"	@!%p6 bra 	$Lt_1_17410;\n"
"	.loc	14	298	0\n"
"	add.f32 	%f39, %f82, %f39;\n"
"	@!%p9 bra 	$Lt_1_17410;\n"
"	.loc	14	300	0\n"
"	add.u64 	%rd43, %rd41, %rd18;\n"
"	mov.f32 	%f85, %f64;\n"
"	ld.shared.f32 	%f86, [%rd43+4];\n"
"	ld.shared.f32 	%f87, [%rd43+0];\n"
"	mul.f32 	%f88, %f87, %f85;\n"
"	sub.f32 	%f89, %f88, %f86;\n"
"	mul.f32 	%f90, %f85, %f89;\n"
"	.loc	14	301	0\n"
"	ld.shared.f32 	%f91, [%rd43+8];\n"
"	sub.f32 	%f92, %f90, %f91;\n"
"	.loc	14	263	0\n"
"	ld.shared.f32 	%f42, [%rd37+0];\n"
"	.loc	14	301	0\n"
"	mad.f32 	%f40, %f42, %f92, %f40;\n"
"$Lt_1_17410:\n"
"$Lt_1_16898:\n"
"	@!%p7 bra 	$Lt_1_17922;\n"
"	.loc	14	305	0\n"
"	mov.f32 	%f93, %f12;\n"
"	mul.f32 	%f94, %f53, %f53;\n"
"	mad.f32 	%f95, %f84, %f94, %f93;\n"
"	mov.f32 	%f12, %f95;\n"
"	.loc	14	306	0\n"
"	mov.f32 	%f96, %f14;\n"
"	mad.f32 	%f97, %f84, %f55, %f96;\n"
"	mov.f32 	%f14, %f97;\n"
"	.loc	14	307	0\n"
"	mov.f32 	%f98, %f16;\n"
"	mul.f32 	%f99, %f54, %f54;\n"
"	mad.f32 	%f100, %f84, %f99, %f98;\n"
"	mov.f32 	%f16, %f100;\n"
"	.loc	14	308	0\n"
"	mov.f32 	%f101, %f18;\n"
"	mul.f32 	%f102, %f52, %f53;\n"
"	mad.f32 	%f103, %f84, %f102, %f101;\n"
"	mov.f32 	%f18, %f103;\n"
"	.loc	14	309	0\n"
"	mov.f32 	%f104, %f20;\n"
"	mul.f32 	%f105, %f53, %f54;\n"
"	mad.f32 	%f106, %f84, %f105, %f104;\n"
"	mov.f32 	%f20, %f106;\n"
"	.loc	14	310	0\n"
"	mul.f32 	%f107, %f52, %f54;\n"
"	mad.f32 	%f21, %f84, %f107, %f21;\n"
"	mov.f32 	%f108, %f21;\n"
"$Lt_1_17922:\n"
"$Lt_1_15362:\n"
"	.loc	14	259	0\n"
"	add.u64 	%rd30, %rd27, %rd30;\n"
"	setp.gt.u64 	%p11, %rd33, %rd30;\n"
"	@%p11 bra 	$Lt_1_15106;\n"
"	bra.uni 	$Lt_1_14594;\n"
"$Lt_1_19714:\n"
"	mov.s32 	%r42, 0;\n"
"	setp.gt.s32 	%p6, %r24, %r42;\n"
"	mov.s32 	%r43, 0;\n"
"	setp.gt.s32 	%p7, %r23, %r43;\n"
"$Lt_1_14594:\n"
"	.loc	14	317	0\n"
"	ld.param.u64 	%rd44, [__cudaparm_kernel_pair_fast_engv];\n"
"	add.u64 	%rd45, %rd44, %rd23;\n"
"	@!%p6 bra 	$Lt_1_18690;\n"
"	.loc	14	319	0\n"
"	st.global.f32 	[%rd45+0], %f40;\n"
"	.loc	14	320	0\n"
"	cvt.u64.s32 	%rd46, %r10;\n"
"	mul.lo.u64 	%rd47, %rd46, 4;\n"
"	add.u64 	%rd45, %rd47, %rd45;\n"
"	.loc	14	321	0\n"
"	st.global.f32 	[%rd45+0], %f39;\n"
"	.loc	14	322	0\n"
"	add.u64 	%rd45, %rd47, %rd45;\n"
"$Lt_1_18690:\n"
"	@!%p7 bra 	$Lt_1_19202;\n"
"	.loc	14	326	0\n"
"	mov.f32 	%f109, %f12;\n"
"	st.global.f32 	[%rd45+0], %f109;\n"
"	.loc	14	327	0\n"
"	cvt.u64.s32 	%rd48, %r10;\n"
"	mul.lo.u64 	%rd49, %rd48, 4;\n"
"	add.u64 	%rd45, %rd49, %rd45;\n"
"	.loc	14	326	0\n"
"	mov.f32 	%f110, %f14;\n"
"	st.global.f32 	[%rd45+0], %f110;\n"
"	.loc	14	327	0\n"
"	add.u64 	%rd45, %rd49, %rd45;\n"
"	.loc	14	326	0\n"
"	mov.f32 	%f111, %f16;\n"
"	st.global.f32 	[%rd45+0], %f111;\n"
"	.loc	14	327	0\n"
"	add.u64 	%rd45, %rd49, %rd45;\n"
"	.loc	14	326	0\n"
"	mov.f32 	%f112, %f18;\n"
"	st.global.f32 	[%rd45+0], %f112;\n"
"	.loc	14	327	0\n"
"	add.u64 	%rd45, %rd49, %rd45;\n"
"	.loc	14	326	0\n"
"	mov.f32 	%f113, %f20;\n"
"	st.global.f32 	[%rd45+0], %f113;\n"
"	add.u64 	%rd50, %rd49, %rd45;\n"
"	st.global.f32 	[%rd50+0], %f21;\n"
"$Lt_1_19202:\n"
"	.loc	14	330	0\n"
"	ld.param.u64 	%rd51, [__cudaparm_kernel_pair_fast_ans];\n"
"	mul.lo.u64 	%rd52, %rd22, 16;\n"
"	add.u64 	%rd53, %rd51, %rd52;\n"
"	mov.f32 	%f114, %f115;\n"
"	st.global.v4.f32 	[%rd53+0], {%f38,%f37,%f36,%f114};\n"
"$Lt_1_14082:\n"
"	.loc	14	332	0\n"
"	exit;\n"
"$LDWend_kernel_pair_fast:\n"
"	}\n"
;
