libdir = ${exec_prefix}/lib${LIBSUFFIX}
pkglibdir = $(libdir)/$(mypkglibdir)

SUBDIRS =

# Apps and files included in BUILT_SOURCES are compiled/created first.
# It must include qpp since qpp is required to generate some cpp's
BUILT_SOURCES = dummy

dummy:
	echo "Build started!"

EXTRA_INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir)/lib
libqore_la_LDFLAGS = -version-info 19:0:7 -no-undefined ${QORE_LIB_LDFLAGS}
AM_CPPFLAGS = $(EXTRA_INCLUDES) ${QORE_LIB_CPPFLAGS}
AM_CXXFLAGS = ${QORE_LIB_CXXFLAGS}
AM_YFLAGS = -d

lib_LTLIBRARIES = libqore.la

bin_PROGRAMS = qpp$(EXEEXT)

qpp_SOURCES = qpp.cpp
qpp_CPPFLAGS = $(EXTRA_INCLUDES) ${QPP_CPPFLAGS}

if COND_NEED_GETOPT_LONG
qpp_SOURCES += getopt_long.cpp
endif

if COND_MINGWCC
BUILT_SOURCES += qpp_native
QPP=./qpp_native
qpp_native: $(qpp_SOURCES)
	$(NATIVE_CXX) $(EXTRA_INCLUDES) $(NATIVE_CPPFLAGS) $(QPP_CPPFLAGS) $(NATIVE_LDFLAGS) -o $@ $(qpp_SOURCES)
else
QPP=./qpp$(EXEEXT)
BUILT_SOURCES += ./qpp$(EXEEXT)
endif

QORE_PSEUDO_SRC = Pseudo_QC_All.cpp Pseudo_QC_Bool.cpp Pseudo_QC_Int.cpp \
	Pseudo_QC_Float.cpp Pseudo_QC_Date.cpp Pseudo_QC_Object.cpp \
	Pseudo_QC_Hash.cpp Pseudo_QC_String.cpp Pseudo_QC_Binary.cpp \
	Pseudo_QC_List.cpp Pseudo_QC_Closure.cpp Pseudo_QC_Callref.cpp \
	Pseudo_QC_Nothing.cpp Pseudo_QC_Number.cpp

QORE_QPP_TARGETS = QC_Queue.cpp QC_Socket.cpp QC_ReadOnlyFile.cpp QC_File.cpp QC_AbstractSmartLock.cpp \
	QC_Mutex.cpp QC_AutoLock.cpp \
	QC_Gate.cpp QC_AutoGate.cpp QC_RWLock.cpp QC_AutoReadLock.cpp QC_AutoWriteLock.cpp \
	QC_Condition.cpp QC_Sequence.cpp QC_Counter.cpp QC_HTTPClient.cpp QC_FtpClient.cpp \
	QC_AbstractIterator.cpp QC_AbstractQuantifiedIterator.cpp \
	QC_AbstractBidirectionalIterator.cpp QC_AbstractQuantifiedBidirectionalIterator.cpp \
	QC_ListIterator.cpp QC_ListReverseIterator.cpp \
	QC_HashIterator.cpp QC_HashReverseIterator.cpp \
	QC_HashKeyIterator.cpp QC_HashKeyReverseIterator.cpp \
	QC_HashPairIterator.cpp QC_HashPairReverseIterator.cpp \
	QC_ObjectIterator.cpp QC_ObjectReverseIterator.cpp \
	QC_ObjectKeyIterator.cpp QC_ObjectKeyReverseIterator.cpp \
	QC_ObjectPairIterator.cpp QC_ObjectPairReverseIterator.cpp \
	QC_HashListIterator.cpp QC_HashListReverseIterator.cpp \
	QC_ListHashIterator.cpp QC_ListHashReverseIterator.cpp \
	QC_AbstractLineIterator.cpp QC_FileLineIterator.cpp QC_DataLineIterator.cpp QC_InputStreamLineIterator.cpp \
	QC_SingleValueIterator.cpp \
	QC_RangeIterator.cpp \
	QC_ThreadPool.cpp \
	QC_TreeMap.cpp \
	QC_AbstractDatasource.cpp \
	QC_AbstractSQLStatement.cpp \
	QC_Datasource.cpp QC_DatasourcePool.cpp QC_SQLStatement.cpp QC_Dir.cpp \
    QC_ProgramControl.cpp QC_Program.cpp QC_DebugProgram.cpp QC_Breakpoint.cpp \
	QC_Expression.cpp \
	QC_GetOpt.cpp QC_TermIOS.cpp QC_TimeZone.cpp QC_SSLCertificate.cpp QC_SSLPrivateKey.cpp \
	QC_AbstractThreadResource.cpp \
	QC_StreamBase.cpp \
	QC_InputStream.cpp QC_OutputStream.cpp \
	QC_BinaryInputStream.cpp QC_BinaryOutputStream.cpp \
	QC_StringInputStream.cpp QC_StringOutputStream.cpp \
	QC_FileInputStream.cpp QC_FileOutputStream.cpp \
	QC_EncodingConversionInputStream.cpp QC_EncodingConversionOutputStream.cpp \
	QC_StreamPipe.cpp QC_PipeInputStream.cpp QC_PipeOutputStream.cpp \
	QC_StreamWriter.cpp QC_StreamReader.cpp QC_BufferedStreamReader.cpp \
	QC_Transform.cpp QC_TransformInputStream.cpp QC_TransformOutputStream.cpp \
	QC_StdoutOutputStream.cpp QC_StderrOutputStream.cpp \
	QC_Serializable.cpp \
    QC_AbstractPollableIoObject.cpp \
    QC_AbstractPollableIoObjectBase.cpp \
    QC_AbstractPollOperation.cpp \
    QC_SocketPollOperationBase.cpp \
    QC_SocketPollOperation.cpp \
    QC_FilePollOperation.cpp \
	ql_misc.cpp ql_compression.cpp ql_thread.cpp ql_crypto.cpp ql_lib.cpp ql_file.cpp \
	ql_string.cpp ql_time.cpp ql_math.cpp ql_list.cpp ql_pwd.cpp ql_object.cpp \
	ql_dbi.cpp ql_context.cpp ql_env.cpp ql_type.cpp \
	qc_option.cpp qc_errno.cpp qc_qore.cpp

QorePseudoMethods.cpp: $(QORE_PSEUDO_SRC)

QORE_GENERATED_SOURCES = $(QORE_QPP_TARGETS) #parser.cpp parser.hpp scanner.cpp

dox-src: $(QORE_GENERATED_SOURCES)

QORE_ALL_SRC = $(QORE_GENERATED_SOURCES) $(QORE_PSEUDO_SRC)

QORE_ALL_QPP_SRC = $(subst cpp,qpp,$(QORE_QPP_TARGETS) $(QORE_PSEUDO_SRC))

$(QORE_QPP_TARGETS) $(QORE_PSEUDO_SRC): %.cpp: %.qpp $(QPP)
	$(QPP) -V $< -o $@

CLEANFILES = $(QORE_ALL_SRC)

# only copy parser.h to parser.hpp as automake < 1.12 will still expect parser.h in the dist file :-(
parser.hpp: dummy
	if [ -f parser.h -a \( ! -f parser.hpp -o \( parser.h -nt parser.hpp \) \) ]; then cp parser.h parser.hpp; fi
parser.cpp: parser.ypp
scanner.cpp: scanner.lpp

if COND_SINGLE_COMPILATION_UNIT
libqore_la_SOURCES = \
	single-compilation-unit.cpp

single-compilation-unit.cpp: parser.cpp parser.hpp scanner.cpp $(QORE_ALL_SRC)

else
nodist_libqore_la_SOURCES = $(QORE_GENERATED_SOURCES)

libqore_la_SOURCES = \
	parser.ypp \
	scanner.lpp \
	BarewordNode.cpp \
	SelfVarrefNode.cpp \
	StaticClassVarRefNode.cpp \
	BackquoteNode.cpp \
	ContextrefNode.cpp \
	ComplexContextrefNode.cpp \
	ContextRowNode.cpp \
	VarRefNode.cpp \
	FunctionCallNode.cpp \
	NewComplexTypeNode.cpp \
	ReferenceNode.cpp \
	lvalue_ref.cpp \
	ScopedRefNode.cpp \
	ClassRefNode.cpp \
	AbstractQoreNode.cpp \
	QoreStringNode.cpp \
	DateTimeNode.cpp \
	qore_date_private.cpp \
	QoreHashNode.cpp \
	BinaryNode.cpp \
	QoreNumberNode.cpp \
	QoreNullNode.cpp \
	QoreNothingNode.cpp \
	Function.cpp \
	GlobalVariableList.cpp \
	FunctionList.cpp \
	AbstractStatement.cpp \
	AssertStatement.cpp \
	DebugStatement.cpp \
	OnBlockExitStatement.cpp \
	ExpressionStatement.cpp \
	ReturnStatement.cpp \
	StatementBlock.cpp \
	ContextStatement.cpp \
	SummarizeStatement.cpp \
	IfStatement.cpp \
	WhileStatement.cpp \
	DoWhileStatement.cpp \
	ForStatement.cpp \
	ForEachStatement.cpp \
	TryStatement.cpp \
	RethrowStatement.cpp \
	ThrowStatement.cpp \
	SwitchStatement.cpp \
	Variable.cpp \
	WeakReferenceNode.cpp \
	WeakHashReferenceNode.cpp \
	WeakListReferenceNode.cpp \
	support.cpp \
	QoreType.cpp \
	ModuleManager.cpp \
	QoreException.cpp \
	ExceptionSink.cpp \
	QoreStandardException.cpp \
	QoreXSinkException.cpp \
	QoreClass.cpp \
	TypedHashDecl.cpp \
	QoreReflection.cpp \
	Context.cpp \
	FindNode.cpp \
	charset.cpp \
	unicode-charmaps.cpp \
	ThreadClosureVariableStack.cpp \
	ThreadLocalVariableData.cpp \
	QoreProgram.cpp \
	QoreProgramHelper.cpp \
	QoreNamespace.cpp \
	QoreNet.cpp \
	QoreURL.cpp \
	QoreFile.cpp \
	QoreDir.cpp \
	QoreSocket.cpp \
	DateTime.cpp \
	QoreLib.cpp \
	QoreTimeZoneManager.cpp \
	QoreString.cpp \
	QoreObject.cpp \
	RSet.cpp \
	RSection.cpp \
	QoreListNode.cpp \
	qore-main.cpp \
	QoreGetOpt.cpp \
	QoreFtpClient.cpp \
	DBI.cpp \
	Datasource.cpp \
	DatasourcePool.cpp \
	SQLStatement.cpp \
	QoreSQLStatement.cpp \
	ManagedDatasource.cpp \
	ReferenceArgumentHelper.cpp \
	ReferenceHelper.cpp \
	ConstantList.cpp \
	QoreClassList.cpp \
	HashDeclList.cpp \
	ThreadResourceList.cpp \
	AbstractThreadResource.cpp \
	thread.cpp \
	VRMutex.cpp \
	VLock.cpp \
	QoreRWLock.cpp \
	AbstractSmartLock.cpp \
	ExecArgList.cpp \
	NamedScope.cpp \
	RWLock.cpp \
	QoreSSLBase.cpp \
	QoreSSLCertificate.cpp \
	QoreSSLPrivateKey.cpp \
	QoreSocketObject.cpp \
	QoreCondition.cpp \
	QoreQueue.cpp \
	QoreQueueHelper.cpp \
	QoreRegex.cpp \
	QoreRegexBase.cpp \
	QoreRegexSubst.cpp \
	QoreTransliteration.cpp \
	Sequence.cpp \
	QoreReferenceCounter.cpp \
	SystemEnvironment.cpp \
	SmartMutex.cpp \
	QoreCounter.cpp \
	CallReferenceNode.cpp \
	QoreParseHashNode.cpp \
	QoreParseListNode.cpp \
	QoreClosureParseNode.cpp \
	QoreClosureNode.cpp \
	QoreImplicitArgumentNode.cpp \
	QoreImplicitElementNode.cpp \
	ParseOptionMap.cpp \
	QoreTypeInfo.cpp \
	QoreDeleteOperatorNode.cpp \
	QoreRemoveOperatorNode.cpp \
	QoreSpliceOperatorNode.cpp \
	QoreExtractOperatorNode.cpp \
	QoreCastOperatorNode.cpp \
	QoreKeysOperatorNode.cpp \
	QoreUnaryMinusOperatorNode.cpp \
	QoreUnaryPlusOperatorNode.cpp \
	QoreLogicalNotOperatorNode.cpp \
	QoreDotEvalOperatorNode.cpp \
	QoreLogicalEqualsOperatorNode.cpp \
	QoreLogicalAbsoluteEqualsOperatorNode.cpp \
	QoreModuloOperatorNode.cpp \
	QoreBinaryAndOperatorNode.cpp \
	QoreBinaryOrOperatorNode.cpp \
	QoreBinaryXorOperatorNode.cpp \
	QoreBinaryNotOperatorNode.cpp \
	QoreShiftLeftOperatorNode.cpp \
	QoreShiftRightOperatorNode.cpp \
	QoreExistsOperatorNode.cpp \
	QoreElementsOperatorNode.cpp \
	QoreInstanceOfOperatorNode.cpp \
	QoreQuestionMarkOperatorNode.cpp \
	QoreHashObjectDereferenceOperatorNode.cpp \
	QoreRegexMatchOperatorNode.cpp \
	QoreRegexNMatchOperatorNode.cpp \
	QoreRegexExtractOperatorNode.cpp \
	QoreRegexSubstOperatorNode.cpp \
	QoreTransliterationOperatorNode.cpp \
	QoreAssignmentOperatorNode.cpp \
	QoreListAssignmentOperatorNode.cpp \
	QorePlusEqualsOperatorNode.cpp \
	QoreIntPlusEqualsOperatorNode.cpp \
	QoreMinusEqualsOperatorNode.cpp \
	QoreIntMinusEqualsOperatorNode.cpp \
	QoreOrEqualsOperatorNode.cpp \
	QoreAndEqualsOperatorNode.cpp \
	QoreModuloEqualsOperatorNode.cpp \
	QoreMultiplyEqualsOperatorNode.cpp \
	QoreDivideEqualsOperatorNode.cpp \
	QoreDivisionOperatorNode.cpp \
	QoreXorEqualsOperatorNode.cpp \
	QoreShiftLeftEqualsOperatorNode.cpp \
	QoreShiftRightEqualsOperatorNode.cpp \
	QorePostIncrementOperatorNode.cpp \
	QorePostDecrementOperatorNode.cpp \
	QorePreIncrementOperatorNode.cpp \
	QorePreDecrementOperatorNode.cpp \
	QoreIntPostIncrementOperatorNode.cpp \
	QoreIntPostDecrementOperatorNode.cpp \
	QoreIntPreIncrementOperatorNode.cpp \
	QoreIntPreDecrementOperatorNode.cpp \
	QoreLogicalLessThanOperatorNode.cpp \
	QoreLogicalGreaterThanOperatorNode.cpp \
	QoreLogicalLessThanOrEqualsOperatorNode.cpp \
	QoreLogicalGreaterThanOrEqualsOperatorNode.cpp \
	QoreMapOperatorNode.cpp \
	QoreMapSelectOperatorNode.cpp \
	QoreHashMapOperatorNode.cpp \
	QoreHashMapSelectOperatorNode.cpp \
	QoreFoldlOperatorNode.cpp \
	QoreSelectOperatorNode.cpp \
	QoreNullCoalescingOperatorNode.cpp \
	QoreValueCoalescingOperatorNode.cpp \
	QoreChompOperatorNode.cpp \
	QoreTrimOperatorNode.cpp \
	QoreSquareBracketsOperatorNode.cpp \
	QoreShiftOperatorNode.cpp \
	QoreUnshiftOperatorNode.cpp \
	QorePopOperatorNode.cpp \
	QorePushOperatorNode.cpp \
	QoreLogicalAndOperatorNode.cpp \
	QoreLogicalOrOperatorNode.cpp \
	QoreLogicalComparisonOperatorNode.cpp \
	QorePlusOperatorNode.cpp \
	QoreMinusOperatorNode.cpp \
	QoreMultiplicationOperatorNode.cpp \
	QoreBackgroundOperatorNode.cpp \
	QoreRangeOperatorNode.cpp \
	QoreSquareBracketsRangeOperatorNode.cpp \
	QorePseudoMethods.cpp \
	QoreHTTPClient.cpp \
	QoreHttpClientObject.cpp \
	QoreValue.cpp \
	StreamPipe.cpp \
	CompressionTransforms.cpp \
	EncryptionTransforms.cpp \
	Transform.cpp \
	xxhash.cpp \
	FunctionalOperatorInterface.cpp \
	FunctionalOperator.cpp \
	QoreSerializable.cpp \
    UnicodeCharacterIterator.cpp \
    QoreEllipsesNode.cpp \
    QoreRegexInterface.cpp \
    QoreThreadLocalStorage.cpp

if COND_SIGNAL_HANDLING
libqore_la_SOURCES += QoreSignal.cpp
endif
if COND_DEBUG
libqore_la_SOURCES += ql_debug.cpp
endif
if COND_NEED_GLOB
libqore_la_SOURCES += glob.cpp
endif
if COND_NEED_INET_NTOP
libqore_la_SOURCES += inet_ntop.cpp
endif
if COND_NEED_INET_PTON
libqore_la_SOURCES += inet_pton.cpp
endif
endif

if COND_SOLARIS_CC_X86_64
libqore_la_SOURCES += sunpro-x86_64.s
endif
if COND_SOLARIS_CC_I386
libqore_la_SOURCES += sunpro-i386.s
endif
if COND_SOLARIS_CC_SPARC32
libqore_la_SOURCES += sunpro-sparc32.s
endif
#if COND_SOLARIS_CC_SPARC64
#libqore_la_SOURCES += sunpro-sparc64.s
#endif
if COND_HPUX_ACC_PARISC32
libqore_la_SOURCES += acc-parisc32.s
endif
if COND_HPUX_ACC_64BIT_IA64
libqore_la_SOURCES += acc-64bit-ia64.s
endif
