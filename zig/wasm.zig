//! A runtime and its modules belong to one thread at a time.

const std = @import("std");

pub const c = @import("wasm_c");

pub const version: std.SemanticVersion = .{
    .major = c.WASM_H_VERSION_MAJOR,
    .minor = c.WASM_H_VERSION_MINOR,
    .patch = c.WASM_H_VERSION_PATCH,
};

pub const page_size = 65536;

/// The C runtime's `wasm_error_t` codes. `Runtime.errorMessage` describes the most recent one.
pub const Error = error{
    InvalidMagic,
    InvalidVersion,
    Malformed,
    InvalidSection,
    TypeMismatch,
    UnknownImport,
    UndefinedExport,
    StackOverflow,
    StackUnderflow,
    OutOfBounds,
    Unreachable,
    IndirectCallTypeMismatch,
    UndefinedElement,
    UninitializedTable,
    DivByZero,
    IntOverflow,
    InvalidConversion,
    CallStackExhausted,
    OutOfMemory,
    OutOfFuel,
    Trap,
    Exit,
};

const error_codes = .{
    .{ c.WASM_ERR_INVALID_MAGIC, error.InvalidMagic },
    .{ c.WASM_ERR_INVALID_VERSION, error.InvalidVersion },
    .{ c.WASM_ERR_MALFORMED, error.Malformed },
    .{ c.WASM_ERR_INVALID_SECTION, error.InvalidSection },
    .{ c.WASM_ERR_TYPE_MISMATCH, error.TypeMismatch },
    .{ c.WASM_ERR_UNKNOWN_IMPORT, error.UnknownImport },
    .{ c.WASM_ERR_UNDEFINED_EXPORT, error.UndefinedExport },
    .{ c.WASM_ERR_STACK_OVERFLOW, error.StackOverflow },
    .{ c.WASM_ERR_STACK_UNDERFLOW, error.StackUnderflow },
    .{ c.WASM_ERR_OUT_OF_BOUNDS, error.OutOfBounds },
    .{ c.WASM_ERR_UNREACHABLE, error.Unreachable },
    .{ c.WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH, error.IndirectCallTypeMismatch },
    .{ c.WASM_ERR_UNDEFINED_ELEMENT, error.UndefinedElement },
    .{ c.WASM_ERR_UNINITIALIZED_TABLE, error.UninitializedTable },
    .{ c.WASM_ERR_DIV_BY_ZERO, error.DivByZero },
    .{ c.WASM_ERR_INT_OVERFLOW, error.IntOverflow },
    .{ c.WASM_ERR_INVALID_CONVERSION, error.InvalidConversion },
    .{ c.WASM_ERR_CALL_STACK_EXHAUSTED, error.CallStackExhausted },
    .{ c.WASM_ERR_OOM, error.OutOfMemory },
    .{ c.WASM_ERR_OUT_OF_FUEL, error.OutOfFuel },
    .{ c.WASM_ERR_TRAP, error.Trap },
    .{ c.WASM_ERR_EXIT, error.Exit },
};

fn check(code: c.wasm_error_t) Error!void {
    if (code == c.WASM_OK) return;
    inline for (error_codes) |entry| {
        if (code == entry[0]) return entry[1];
    }
    return error.Trap;
}

/// The `wasm_error_t` a host function reports for `err`; errors outside `Error` become traps.
pub fn errorCode(err: anyerror) c.wasm_error_t {
    inline for (error_codes) |entry| {
        if (err == entry[1]) return entry[0];
    }
    return c.WASM_ERR_TRAP;
}

pub const ValType = enum(c.wasm_valtype_t) {
    i32 = c.WASM_TYPE_I32,
    i64 = c.WASM_TYPE_I64,
    f32 = c.WASM_TYPE_F32,
    f64 = c.WASM_TYPE_F64,
    v128 = c.WASM_TYPE_V128,
    funcref = c.WASM_TYPE_FUNCREF,
    externref = c.WASM_TYPE_EXTERNREF,
    anyref = c.WASM_TYPE_ANYREF,
    eqref = c.WASM_TYPE_EQREF,
    i31ref = c.WASM_TYPE_I31REF,
    structref = c.WASM_TYPE_STRUCTREF,
    arrayref = c.WASM_TYPE_ARRAYREF,
    exnref = c.WASM_TYPE_EXNREF,
    nullref = c.WASM_TYPE_NONE,
    nullfuncref = c.WASM_TYPE_NOFUNC,
    nullexternref = c.WASM_TYPE_NOEXTERN,
    nullexnref = c.WASM_TYPE_NOEXN,
    void = c.WASM_TYPE_VOID,
    _,

    /// The Wasm type a Zig value of type `T` maps to, or null for `Value`, which carries its own.
    pub fn of(comptime T: type) ?ValType {
        return switch (T) {
            Value => null,
            i32, u32 => .i32,
            i64, u64 => .i64,
            f32 => .f32,
            f64 => .f64,
            V128 => .v128,
            FuncRef => .funcref,
            ExternRef => .externref,
            else => if (comptime isExternPointer(T)) .externref else @compileError("no Wasm value type for " ++ @typeName(T)),
        };
    }

    /// Whether a value of type `actual` is acceptable where `self` is expected.
    fn accepts(self: ValType, actual: ValType) bool {
        return self == actual or switch (self) {
            .funcref => actual == .nullfuncref,
            .externref => actual == .nullexternref,
            else => false,
        };
    }
};

/// The bytes of a v128 in memory order, lane 0 first.
pub const V128 = @Vector(16, u8);

/// A function reference: a function index in the module that produced it.
pub const FuncRef = enum(u32) {
    null = std.math.maxInt(u32),
    _,

    pub fn index(self: FuncRef) ?u32 {
        return if (self == .null) null else @backingInt(self);
    }
};

/// A host value carried through Wasm as an externref. Optional pointers convert implicitly.
pub const ExternRef = enum(usize) {
    null = 0,
    _,

    pub fn fromPtr(ptr: ?*const anyopaque) ExternRef {
        return @fromBackingInt(@intFromPtr(ptr));
    }

    pub fn toPtr(self: ExternRef, comptime T: type) ?*T {
        return @ptrFromInt(@backingInt(self));
    }
};

fn isExternPointer(comptime T: type) bool {
    return switch (@typeInfo(T)) {
        .optional => |optional| switch (@typeInfo(optional.child)) {
            .pointer => |pointer| pointer.size == .one,
            else => false,
        },
        else => false,
    };
}

/// `wasm_value_t` with Zig conversions. The layout matches the C struct.
pub const Value = extern struct {
    kind: ValType,
    of: extern union {
        i32: i32,
        i64: i64,
        f32: f32,
        f64: f64,
        v128: [16]u8,
        funcref: u32,
        externref: usize,
        gc_ref: usize,
        gc_obj: ?*anyopaque,
    },

    comptime {
        std.debug.assert(@sizeOf(Value) == @sizeOf(c.wasm_value_t));
        std.debug.assert(@alignOf(Value) == @alignOf(c.wasm_value_t));
        std.debug.assert(@offsetOf(Value, "of") == @offsetOf(c.wasm_value_t, "of"));
    }

    pub fn from(x: anytype) Value {
        const T = @TypeOf(x);
        if (T == Value) return x;
        if (T == comptime_int) @compileError("pass a sized integer, e.g. @as(i32, 1)");
        if (T == comptime_float) @compileError("pass a sized float, e.g. @as(f64, 1.0)");
        var v: Value = .{ .kind = ValType.of(T).?, .of = .{ .i64 = 0 } };
        switch (T) {
            i32 => v.of.i32 = x,
            u32 => v.of.i32 = @bitCast(x),
            i64 => v.of.i64 = x,
            u64 => v.of.i64 = @bitCast(x),
            f32 => v.of.f32 = x,
            f64 => v.of.f64 = x,
            V128 => v.of.v128 = x,
            FuncRef => v.of.funcref = @backingInt(x),
            ExternRef => v.of.externref = @backingInt(x),
            else => v.of.externref = @intFromPtr(x),
        }
        return v;
    }

    /// Unwraps the value as `T`, failing when its Wasm type differs.
    pub fn to(self: Value, comptime T: type) Error!T {
        if (T == Value) return self;
        if (!ValType.of(T).?.accepts(self.kind)) return error.TypeMismatch;
        return switch (T) {
            i32 => self.of.i32,
            u32 => @bitCast(self.of.i32),
            i64 => self.of.i64,
            u64 => @bitCast(self.of.i64),
            f32 => self.of.f32,
            f64 => self.of.f64,
            V128 => self.of.v128,
            FuncRef => @fromBackingInt(self.of.funcref),
            ExternRef => @fromBackingInt(self.of.externref),
            else => @ptrFromInt(self.of.externref),
        };
    }

    pub fn @"null"(kind: ValType) Value {
        return fromC(c.wasm_ref_null(@backingInt(kind)));
    }

    pub fn i31ref(x: i32) Value {
        return fromC(c.wasm_i31ref(x));
    }

    fn fromC(value: c.wasm_value_t) Value {
        return @as(*const Value, @ptrCast(&value)).*;
    }

    fn toC(self: Value) c.wasm_value_t {
        return @as(*const c.wasm_value_t, @ptrCast(&self)).*;
    }

    fn cPtr(values: []const Value) [*c]const c.wasm_value_t {
        return @ptrCast(values.ptr);
    }

    fn cPtrMut(values: []Value) [*c]c.wasm_value_t {
        return @ptrCast(values.ptr);
    }
};

/// A function signature as the runtime reports it. Slices point into the module.
pub const FuncType = struct {
    params: []const ValType,
    results: []const ValType,

    fn fromC(ft: ?*const c.wasm_functype_t) ?FuncType {
        const t = ft orelse return null;
        return .{
            .params = valTypes(t.params, t.num_params),
            .results = valTypes(t.results, t.num_results),
        };
    }

    fn valTypes(ptr: [*c]const c.wasm_valtype_t, len: u32) []const ValType {
        if (len == 0) return &.{};
        const types: [*]const ValType = @ptrCast(ptr);
        return types[0..len];
    }

    /// `Value` parameters and results match any Wasm type.
    pub fn matches(self: FuncType, comptime Fn: type) bool {
        const sig = Signature(Fn);
        if (self.params.len != sig.params.len or self.results.len != sig.results.len) return false;
        inline for (sig.params, 0..) |T, i| {
            if (ValType.of(T)) |expected| {
                if (expected != self.params[i]) return false;
            }
        }
        inline for (sig.results, 0..) |T, i| {
            if (ValType.of(T)) |expected| {
                if (expected != self.results[i]) return false;
            }
        }
        return true;
    }
};

/// The Wasm parameter and result types of a Zig function type. Results come from the return type,
/// with any error set removed: `void` is none, a tuple is several, anything else is one.
fn Signature(comptime Fn: type) type {
    const info = @typeInfo(Fn).@"fn";
    const Return = info.return_type orelse @compileError("generic return types are not supported");
    const Payload = switch (@typeInfo(Return)) {
        .error_union => |eu| eu.payload,
        else => Return,
    };
    const result_types: []const type = if (Payload == void)
        &.{}
    else switch (@typeInfo(Payload)) {
        .@"struct" => |s| if (s.is_tuple) s.field_types else &.{Payload},
        else => &.{Payload},
    };
    var param_types: [info.param_types.len]type = undefined;
    for (info.param_types, 0..) |T, i| param_types[i] = T orelse @compileError("anytype parameters are not supported");
    const frozen_params = param_types;
    return struct {
        pub const params: []const type = &frozen_params;
        pub const results = result_types;
        pub const Result = Payload;
        pub const returns_error = Payload != Return;

        fn valTypes(comptime types: []const type) [types.len]c.wasm_valtype_t {
            var out: [types.len]c.wasm_valtype_t = undefined;
            inline for (types, 0..) |T, i| {
                out[i] = @backingInt(ValType.of(T) orelse @compileError("host functions need concrete Wasm types, not Value"));
            }
            return out;
        }

        fn packResults(result: Payload, out: []Value) void {
            if (results.len == 0) {} else if (results.len == 1 and results[0] == Payload) {
                out[0] = .from(result);
            } else {
                inline for (0..results.len) |i| out[i] = .from(result[i]);
            }
        }

        fn unpackResults(values: []const Value) Error!Payload {
            if (results.len == 0) {
                return;
            } else if (results.len == 1 and results[0] == Payload) {
                return values[0].to(Payload);
            } else {
                var result: Payload = undefined;
                inline for (0..results.len) |i| result[i] = try values[i].to(results[i]);
                return result;
            }
        }
    };
}

/// Overrides for `wasm_config_default`; null keeps the default.
pub const Config = struct {
    max_stack_values: ?u32 = null,
    max_call_depth: ?u32 = null,
    max_labels: ?u32 = null,
    initial_gc_heap_size: ?usize = null,
    frame_arena_size: ?usize = null,
    /// Defer validating each function body until its first call.
    lazy_validation: ?bool = null,
    /// Locals per function, parameters included; 0 is unlimited.
    max_function_locals: ?u32 = null,
    /// Bytes per linear memory, at instantiation and on growth; 0 is unlimited.
    max_memory_bytes: ?u64 = null,
    /// Elements per table, at instantiation and on growth; 0 is unlimited.
    max_table_elements: ?u64 = null,
};

/// Proposal feature flags, bit-compatible with `WASM_FEATURE_*`.
pub const Features = packed struct(u32) {
    sign_ext: bool = false,
    nontrapping_fma: bool = false,
    multi_value: bool = false,
    mutable_globals: bool = false,
    bulk_memory: bool = false,
    reference_types: bool = false,
    multi_memory: bool = false,
    extended_const: bool = false,
    tail_call: bool = false,
    exceptions: bool = false,
    simd: bool = false,
    gc: bool = false,
    memory64: bool = false,
    wide_arithmetic: bool = false,
    custom_page_sizes: bool = false,
    compact_imports: bool = false,
    threads: bool = false,
    reserved: u15 = 0,

    comptime {
        for (.{
            .{ "sign_ext", c.WASM_FEATURE_SIGN_EXT },
            .{ "nontrapping_fma", c.WASM_FEATURE_NONTRAPPING_FMA },
            .{ "multi_value", c.WASM_FEATURE_MULTI_VALUE },
            .{ "mutable_globals", c.WASM_FEATURE_MUTABLE_GLOBALS },
            .{ "bulk_memory", c.WASM_FEATURE_BULK_MEMORY },
            .{ "reference_types", c.WASM_FEATURE_REFERENCE_TYPES },
            .{ "multi_memory", c.WASM_FEATURE_MULTI_MEMORY },
            .{ "extended_const", c.WASM_FEATURE_EXTENDED_CONST },
            .{ "tail_call", c.WASM_FEATURE_TAIL_CALL },
            .{ "exceptions", c.WASM_FEATURE_EXCEPTIONS },
            .{ "simd", c.WASM_FEATURE_SIMD },
            .{ "gc", c.WASM_FEATURE_GC },
            .{ "memory64", c.WASM_FEATURE_MEMORY64 },
            .{ "wide_arithmetic", c.WASM_FEATURE_WIDE_ARITHMETIC },
            .{ "custom_page_sizes", c.WASM_FEATURE_CUSTOM_PAGE_SIZES },
            .{ "compact_imports", c.WASM_FEATURE_COMPACT_IMPORTS },
            .{ "threads", c.WASM_FEATURE_THREADS },
        }) |entry| {
            if (@as(u32, 1) << @bitOffsetOf(Features, entry[0]) != entry[1]) @compileError("feature bit mismatch: " ++ entry[0]);
        }
    }

    fn bits(self: Features) u32 {
        return @bitCast(self);
    }
};

pub const ExternKind = enum(c.wasm_export_kind_t) {
    func = c.WASM_EXPORT_FUNC,
    table = c.WASM_EXPORT_TABLE,
    memory = c.WASM_EXPORT_MEM,
    global = c.WASM_EXPORT_GLOBAL,
    tag = c.WASM_EXPORT_TAG,
    _,
};

pub const Export = struct {
    name: []const u8,
    kind: ExternKind,
    index: u32,
};

pub const Import = struct {
    module: [:0]const u8,
    name: [:0]const u8,
    kind: ExternKind,
    /// The index within the imported kind's index space.
    index: u32,
    /// The function or tag type index, for function and tag imports.
    type_index: u32,
    /// The value or element type, for global and table imports.
    type: ValType,
    mutable: bool,
};

pub const CustomSection = struct {
    name: [:0]const u8,
    data: []const u8,
};

pub const TypeKind = enum(c.wasm_comptype_kind_t) {
    func = c.WASM_COMP_FUNC,
    @"struct" = c.WASM_COMP_STRUCT,
    array = c.WASM_COMP_ARRAY,
    invalid = c.WASM_COMP_INVALID,
    _,
};

pub const CallFrame = struct {
    func_index: u32,
    offset: u32,
};

fn span(ptr: [*c]const u8) [:0]const u8 {
    if (ptr == null) return "";
    return std.mem.span(@as([*:0]const u8, @ptrCast(ptr)));
}

pub const Runtime = opaque {
    pub fn init(config: Config) Error!*Runtime {
        var cfg: c.wasm_config_t = undefined;
        c.wasm_config_default(&cfg);
        if (config.max_stack_values) |v| cfg.max_stack_values = v;
        if (config.max_call_depth) |v| cfg.max_call_depth = v;
        if (config.max_labels) |v| cfg.max_labels = v;
        if (config.initial_gc_heap_size) |v| cfg.initial_gc_heap_size = v;
        if (config.frame_arena_size) |v| cfg.frame_arena_size = v;
        if (config.lazy_validation) |v| cfg.lazy_validation = @intFromBool(v);
        if (config.max_function_locals) |v| cfg.max_function_locals = v;
        if (config.max_memory_bytes) |v| cfg.max_memory_bytes = v;
        if (config.max_table_elements) |v| cfg.max_table_elements = v;
        const rt: *Runtime = @ptrCast(c.wasm_runtime_new(&cfg) orelse return error.OutOfMemory);
        check(c.wasm_runtime_last_error(rt.raw())) catch |err| {
            rt.deinit();
            return err;
        };
        return rt;
    }

    /// Free its modules first.
    pub fn deinit(rt: *Runtime) void {
        c.wasm_runtime_free(rt.raw());
    }

    pub fn raw(rt: *Runtime) *c.wasm_runtime_t {
        return @ptrCast(rt);
    }

    /// Describes the most recent error; empty when there is none.
    pub fn errorMessage(rt: *Runtime) [:0]const u8 {
        return span(c.wasm_runtime_error_message(rt.raw()));
    }

    pub fn clearError(rt: *Runtime) void {
        c.wasm_runtime_clear_error(rt.raw());
    }

    pub fn features(rt: *Runtime) Features {
        return @bitCast(c.wasm_runtime_enabled_features(rt.raw()));
    }

    /// Replaces the enabled feature set. Applies to modules loaded afterwards.
    pub fn setFeatures(rt: *Runtime, set: Features) void {
        c.wasm_runtime_set_enabled_features(rt.raw(), set.bits());
    }

    pub fn enableFeatures(rt: *Runtime, set: Features) void {
        c.wasm_enable_feature(rt.raw(), set.bits());
    }

    pub fn disableFeatures(rt: *Runtime, set: Features) void {
        c.wasm_disable_feature(rt.raw(), set.bits());
    }

    pub fn enableAllFeatures(rt: *Runtime) void {
        c.wasm_enable_all_features(rt.raw());
    }

    /// `bytes` is copied and may be freed afterwards.
    pub fn load(rt: *Runtime, bytes: []const u8) Error!*Module {
        const mod = c.wasm_load(rt.raw(), bytes.ptr, bytes.len) orelse {
            check(c.wasm_runtime_last_error(rt.raw())) catch |err| return err;
            return error.Malformed;
        };
        return @ptrCast(mod);
    }

    /// Decodes and validates `bytes` without resolving imports or running code. The result supports
    /// introspection but not calls; create instances with `Module.instantiate`.
    pub fn compile(rt: *Runtime, bytes: []const u8) Error!*Module {
        const mod = c.wasm_compile(rt.raw(), bytes.ptr, bytes.len) orelse {
            check(c.wasm_runtime_last_error(rt.raw())) catch |err| return err;
            return error.Malformed;
        };
        return @ptrCast(mod);
    }

    /// Binds a Zig function as the import `module_name.name` for modules loaded afterwards.
    ///
    /// `func` may take a leading `*Module` (the caller) followed by Wasm-typed parameters, and may return
    /// `void`, one Wasm-typed value, a tuple of them, or an error union of those. A returned error traps with
    /// its name as the message: members of `Error` keep their code, anything else becomes `error.Trap`.
    pub fn bindFunc(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, comptime func: anytype) Error!void {
        return rt.bindHost(module_name, name, null, HostFunc(void, func));
    }

    /// Like `bindFunc`, with `context` (a single-item pointer) passed as `func`'s first parameter.
    /// `context` must outlive every module that imports the function.
    pub fn bindFuncContext(
        rt: *Runtime,
        module_name: [:0]const u8,
        name: [:0]const u8,
        context: anytype,
        comptime func: anytype,
    ) Error!void {
        const Context = @TypeOf(context);
        if (@typeInfo(Context) != .pointer or @typeInfo(Context).pointer.size != .one)
            @compileError("context must be a single-item pointer, found " ++ @typeName(Context));
        return rt.bindHost(module_name, name, @ptrCast(@constCast(context)), HostFunc(Context, func));
    }

    fn bindHost(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, userdata: ?*anyopaque, comptime Host: type) Error!void {
        var imp = std.mem.zeroes(c.wasm_import_t);
        imp.module = module_name.ptr;
        imp.name = name.ptr;
        imp.type.num_params = Host.param_count;
        imp.type.num_results = Host.result_count;
        // Never null, even when empty: an all-zero signature tells the runtime to accept any import type.
        imp.type.params = @constCast(&Host.params);
        imp.type.results = @constCast(&Host.results);
        imp.func = Host.call;
        imp.userdata = userdata;
        try check(c.wasm_register_import(rt.raw(), &imp));
    }

    /// Binds `value` as the import global `module_name.name`; its kind gives the global's type.
    /// `value` must outlive every module that imports it, which read and write it in place.
    pub fn bindGlobal(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, value: *Value, mutable: bool) Error!void {
        var imp = std.mem.zeroes(c.wasm_global_import_t);
        imp.module = module_name.ptr;
        imp.name = name.ptr;
        imp.type = @backingInt(value.kind);
        imp.is_mutable = @intFromBool(mutable);
        imp.value = @ptrCast(value);
        try check(c.wasm_register_global_import(rt.raw(), &imp));
    }

    pub fn bindMemory(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, memory: *c.wasm_memory_t) Error!void {
        const imp: c.wasm_memory_import_t = .{ .module = module_name.ptr, .name = name.ptr, .memory = memory };
        try check(c.wasm_register_memory_import(rt.raw(), &imp));
    }

    pub fn bindTable(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, table: *c.wasm_table_t) Error!void {
        const imp: c.wasm_table_import_t = .{ .module = module_name.ptr, .name = name.ptr, .table = table };
        try check(c.wasm_register_table_import(rt.raw(), &imp));
    }

    pub fn bindTag(rt: *Runtime, imp: *const c.wasm_tag_import_t) Error!void {
        try check(c.wasm_register_tag_import(rt.raw(), imp));
    }

    /// Binds the partial `wasi_snapshot_preview1` imports. Needs the `platform` build option.
    pub fn bindWasi(rt: *Runtime) Error!void {
        try check(c.wasm_bind_wasi_stubs(rt.raw()));
    }

    /// Binds Emscripten's `env` imports, some of which reach the host filesystem: use only with trusted
    /// modules. Needs the `platform` build option.
    pub fn bindEmscripten(rt: *Runtime) Error!void {
        try check(c.wasm_bind_emscripten_stubs(rt.raw()));
    }

    pub fn collectGarbage(rt: *Runtime) void {
        c.wasm_gc_collect(rt.raw());
    }

    /// Limits execution to `fuel` units; calls fail with `error.OutOfFuel` once it runs out.
    pub fn setFuel(rt: *Runtime, fuel_units: u64) void {
        c.wasm_set_fuel(rt.raw(), fuel_units);
    }

    pub fn fuel(rt: *Runtime) u64 {
        return c.wasm_get_fuel(rt.raw());
    }

    /// The status the guest passed to WASI `proc_exit` when a call failed with `error.Exit`.
    pub fn exitCode(rt: *Runtime) i32 {
        return c.wasm_runtime_exit_code(rt.raw());
    }

    /// Writes the current Wasm call stack into `buffer`, typically from a host function.
    pub fn backtrace(rt: *Runtime, buffer: []u8) [:0]const u8 {
        if (buffer.len == 0) return "";
        c.wasm_dump_backtrace(rt.raw(), buffer.ptr, buffer.len);
        return span(buffer.ptr);
    }

    pub fn callDepth(rt: *Runtime) u32 {
        return c.wasm_get_call_stack_depth(rt.raw());
    }

    pub fn callFrame(rt: *Runtime, depth: u32) ?CallFrame {
        var frame: CallFrame = undefined;
        check(c.wasm_get_call_frame_info(rt.raw(), depth, &frame.func_index, &frame.offset)) catch return null;
        return frame;
    }
};

/// The C callback for a bound Zig function. `Context` is `void` when there is no context parameter.
fn HostFunc(comptime Context: type, comptime func: anytype) type {
    const Fn = @TypeOf(func);
    const sig = Signature(Fn);
    const context_count: usize = @intFromBool(Context != void);
    if (context_count == 1 and (sig.params.len == 0 or sig.params[0] != Context))
        @compileError("the first parameter of a bound function must be the context type " ++ @typeName(Context));
    const has_caller = sig.params.len > context_count and sig.params[context_count] == *Module;
    const first = context_count + @intFromBool(has_caller);
    const wasm_params = sig.params[first..];

    return struct {
        const param_count = wasm_params.len;
        const result_count = sig.results.len;
        const params = sig.valTypes(wasm_params) ++ [1]c.wasm_valtype_t{0};
        const results = sig.valTypes(sig.results) ++ [1]c.wasm_valtype_t{0};

        fn call(
            mod: ?*c.wasm_module_t,
            args: [*c]const c.wasm_value_t,
            num_args: u32,
            out: [*c]c.wasm_value_t,
            num_results: u32,
            userdata: ?*anyopaque,
        ) callconv(.c) c.wasm_error_t {
            if (num_args != wasm_params.len or num_results != sig.results.len) return c.WASM_ERR_TYPE_MISMATCH;
            var call_args: std.meta.ArgsTuple(Fn) = undefined;
            if (context_count == 1) call_args[0] = @ptrCast(@alignCast(userdata.?));
            if (has_caller) call_args[context_count] = @ptrCast(mod.?);
            inline for (wasm_params, 0..) |T, i| {
                const values: [*]const Value = @ptrCast(args);
                call_args[first + i] = values[i].to(T) catch return c.WASM_ERR_TYPE_MISMATCH;
            }
            const returned = @call(.auto, func, call_args);
            const result = if (sig.returns_error) returned catch |err| {
                const code = errorCode(err);
                c.wasm_runtime_set_error(@constCast(c.wasm_module_runtime(mod)), code, @errorName(err));
                return code;
            } else returned;
            if (sig.results.len > 0) {
                const out_values: [*]Value = @ptrCast(out);
                sig.packResults(result, out_values[0..sig.results.len]);
            }
            return c.WASM_OK;
        }
    };
}

/// An exported function with a signature checked against `Fn`, e.g. `fn (i32, i32) i32`.
pub fn TypedFunc(comptime Fn: type) type {
    const sig = Signature(Fn);
    return struct {
        module: *Module,
        index: u32,

        const Self = @This();

        pub fn call(self: Self, args: std.meta.ArgsTuple(Fn)) Error!sig.Result {
            var params: [sig.params.len]Value = undefined;
            inline for (0..sig.params.len) |i| params[i] = .from(args[i]);
            var results: [sig.results.len]Value = undefined;
            try self.module.callIndex(self.index, &params, &results);
            return sig.unpackResults(&results);
        }
    };
}

pub const Module = opaque {
    /// Free modules before the runtime that loaded them.
    pub fn deinit(mod: *Module) void {
        c.wasm_free_module(mod.raw());
    }

    pub fn raw(mod: *Module) *c.wasm_module_t {
        return @ptrCast(mod);
    }

    /// Creates an instance of a module from `Runtime.compile`, resolving the imports bound on its
    /// runtime. Free each instance and the compiled module separately.
    pub fn instantiate(mod: *Module) Error!*Module {
        const instance = c.wasm_instantiate(mod.raw()) orelse {
            check(c.wasm_runtime_last_error(c.wasm_module_runtime(mod.raw()))) catch |err| return err;
            return error.Malformed;
        };
        return @ptrCast(instance);
    }

    pub fn runtime(mod: *Module) *Runtime {
        return @ptrCast(@constCast(c.wasm_module_runtime(mod.raw())));
    }

    fn fail(mod: *Module, err: Error, message: [:0]const u8) Error {
        c.wasm_runtime_set_error(mod.runtime().raw(), errorCode(err), message);
        return err;
    }

    /// Calls an exported function with untyped values; `results.len` must match its result count.
    pub fn call(mod: *Module, name: [:0]const u8, args: []const Value, results: []Value) Error!void {
        try check(c.wasm_call(mod.raw(), name.ptr, Value.cPtr(args), @intCast(args.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    pub fn callIndex(mod: *Module, func_index: u32, args: []const Value, results: []Value) Error!void {
        try check(c.wasm_call_index(mod.raw(), func_index, Value.cPtr(args), @intCast(args.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    /// Looks up an exported function and checks that it has `Fn`'s signature, where a `Value`
    /// parameter or result accepts any type.
    pub fn func(mod: *Module, name: []const u8, comptime Fn: type) Error!TypedFunc(Fn) {
        const exp = mod.findExport(name) orelse return mod.fail(error.UndefinedExport, "no export with that name");
        if (exp.kind != .func) return mod.fail(error.TypeMismatch, "export is not a function");
        return mod.funcAt(exp.index, Fn);
    }

    pub fn funcAt(mod: *Module, func_index: u32, comptime Fn: type) Error!TypedFunc(Fn) {
        const ft = mod.funcType(func_index) orelse return mod.fail(error.UndefinedExport, "function index out of range");
        if (!ft.matches(Fn)) return mod.fail(error.TypeMismatch, "function signature does not match " ++ @typeName(Fn));
        return .{ .module = mod, .index = func_index };
    }

    pub fn getGlobal(mod: *Module, comptime T: type, name: [:0]const u8) Error!T {
        var value: Value = undefined;
        try check(c.wasm_global_get(mod.raw(), name.ptr, @ptrCast(&value)));
        return value.to(T);
    }

    pub fn setGlobal(mod: *Module, name: [:0]const u8, value: anytype) Error!void {
        try check(c.wasm_global_set(mod.raw(), name.ptr, Value.from(value).toC()));
    }

    pub fn globalCount(mod: *Module) u32 {
        return c.wasm_global_count(mod.raw());
    }

    pub fn globalType(mod: *Module, global_index: u32) ValType {
        return @fromBackingInt(c.wasm_global_type(mod.raw(), global_index));
    }

    pub fn globalIsMutable(mod: *Module, global_index: u32) bool {
        return c.wasm_global_is_mutable(mod.raw(), global_index) != 0;
    }

    pub fn memoryCount(mod: *Module) u32 {
        return c.wasm_memory_count(mod.raw());
    }

    pub fn memory(mod: *Module, memory_index: u32) ?Memory {
        if (memory_index >= mod.memoryCount()) return null;
        return .{ .module = mod, .index = memory_index };
    }

    pub fn tableCount(mod: *Module) u32 {
        return c.wasm_table_count(mod.raw());
    }

    pub fn table(mod: *Module, table_index: u32) ?*c.wasm_table_t {
        return c.wasm_table_ref_at(mod.raw(), table_index);
    }

    pub fn exportCount(mod: *Module) u32 {
        return c.wasm_export_count(mod.raw());
    }

    pub fn exportAt(mod: *Module, export_index: u32) Export {
        var len: usize = 0;
        const name = c.wasm_export_name_bytes(mod.raw(), export_index, &len);
        return .{
            .name = if (name == null) "" else name[0..len],
            .kind = @fromBackingInt(c.wasm_export_kind(mod.raw(), export_index)),
            .index = c.wasm_export_index(mod.raw(), export_index),
        };
    }

    pub fn findExport(mod: *Module, name: []const u8) ?Export {
        var kind: c.wasm_export_kind_t = undefined;
        var index: u32 = undefined;
        if (c.wasm_find_export_bytes(mod.raw(), name.ptr, name.len, &kind, &index) == 0) return null;
        return .{ .name = name, .kind = @fromBackingInt(kind), .index = index };
    }

    pub fn exports(mod: *Module) Iterator(Export, exportCount, exportAt) {
        return .{ .module = mod };
    }

    pub fn importCount(mod: *Module) u32 {
        return c.wasm_import_count(mod.raw());
    }

    pub fn importAt(mod: *Module, import_index: u32) Import {
        const info = c.wasm_import_info(mod.raw(), import_index).*;
        return .{
            .module = span(info.module),
            .name = span(info.name),
            .kind = @fromBackingInt(@intCast(info.kind)),
            .index = info.index,
            .type_index = info.type_index,
            .type = @fromBackingInt(info.type),
            .mutable = info.is_mutable != 0,
        };
    }

    pub fn imports(mod: *Module) Iterator(Import, importCount, importAt) {
        return .{ .module = mod };
    }

    pub fn customSectionCount(mod: *Module) u32 {
        return c.wasm_custom_section_count(mod.raw());
    }

    pub fn customSectionAt(mod: *Module, section_index: u32) CustomSection {
        var len: usize = 0;
        const data = c.wasm_custom_section_data(mod.raw(), section_index, &len);
        return .{
            .name = span(c.wasm_custom_section_name(mod.raw(), section_index)),
            .data = if (data == null) &.{} else data[0..len],
        };
    }

    pub fn customSections(mod: *Module) Iterator(CustomSection, customSectionCount, customSectionAt) {
        return .{ .module = mod };
    }

    /// The contents of the first custom section called `name`.
    pub fn customSection(mod: *Module, name: [:0]const u8) ?[]const u8 {
        var len: usize = 0;
        const data = c.wasm_custom_section_find(mod.raw(), name.ptr, &len);
        return if (data == null) null else data[0..len];
    }

    pub fn funcCount(mod: *Module) u32 {
        return c.wasm_func_count(mod.raw());
    }

    pub fn funcType(mod: *Module, func_index: u32) ?FuncType {
        return .fromC(c.wasm_func_functype(mod.raw(), func_index));
    }

    pub fn typeCount(mod: *Module) u32 {
        return c.wasm_type_count(mod.raw());
    }

    pub fn typeKind(mod: *Module, type_index: u32) TypeKind {
        return @fromBackingInt(c.wasm_type_kind(mod.raw(), type_index));
    }

    /// The signature of function type `type_index`, or null for struct and array types.
    pub fn typeFunc(mod: *Module, type_index: u32) ?FuncType {
        return .fromC(c.wasm_type_functype(mod.raw(), type_index));
    }

    pub fn tagCount(mod: *Module) u32 {
        return c.wasm_tag_count(mod.raw());
    }

    pub fn tagType(mod: *Module, tag_index: u32) ?FuncType {
        return .fromC(c.wasm_tag_functype(mod.raw(), tag_index));
    }

    /// Allocates a GC struct of type `type_index`. GC references stay valid only while Wasm can reach them.
    pub fn structNew(mod: *Module, type_index: u32, fields: []const Value) Error!Value {
        var ref: Value = undefined;
        try check(c.wasm_struct_new(mod.raw(), type_index, Value.cPtr(fields), @intCast(fields.len), @ptrCast(&ref)));
        return ref;
    }

    pub fn structGet(mod: *Module, ref: Value, field_index: u32) Error!Value {
        var value: Value = undefined;
        try check(c.wasm_struct_get_field(mod.raw(), ref.toC(), field_index, @ptrCast(&value)));
        return value;
    }

    pub fn structSet(mod: *Module, ref: Value, field_index: u32, value: Value) Error!void {
        try check(c.wasm_struct_set_field(mod.raw(), ref.toC(), field_index, value.toC()));
    }

    pub fn arrayNew(mod: *Module, type_index: u32, elems: []const Value) Error!Value {
        var ref: Value = undefined;
        try check(c.wasm_array_new(mod.raw(), type_index, Value.cPtr(elems), @intCast(elems.len), @ptrCast(&ref)));
        return ref;
    }

    pub fn arrayLen(mod: *Module, ref: Value) Error!u32 {
        var len: u32 = undefined;
        try check(c.wasm_array_length(mod.raw(), ref.toC(), &len));
        return len;
    }

    pub fn arrayGet(mod: *Module, ref: Value, elem_index: u32) Error!Value {
        var value: Value = undefined;
        try check(c.wasm_array_get_elem(mod.raw(), ref.toC(), elem_index, @ptrCast(&value)));
        return value;
    }

    pub fn arraySet(mod: *Module, ref: Value, elem_index: u32, value: Value) Error!void {
        try check(c.wasm_array_set_elem(mod.raw(), ref.toC(), elem_index, value.toC()));
    }

    pub fn setUserdata(mod: *Module, ptr: ?*anyopaque) void {
        c.wasm_module_set_userdata(mod.raw(), ptr);
    }

    pub fn userdata(mod: *Module, comptime T: type) ?*T {
        return @ptrCast(@alignCast(c.wasm_module_get_userdata(mod.raw())));
    }
};

pub fn Iterator(comptime T: type, comptime count: fn (*Module) u32, comptime at: fn (*Module, u32) T) type {
    return struct {
        module: *Module,
        index: u32 = 0,

        pub fn next(self: *@This()) ?T {
            if (self.index >= count(self.module)) return null;
            defer self.index += 1;
            return at(self.module, self.index);
        }
    };
}

/// One of a module's linear memories, defined or imported.
pub const Memory = struct {
    module: *Module,
    index: u32,

    /// The memory's bytes. Growing the memory invalidates the slice unless the memory is shared.
    pub fn data(self: Memory) []u8 {
        const ptr = c.wasm_memory_data_at(self.module.raw(), self.index);
        return if (ptr == null) &.{} else ptr[0..@intCast(self.size())];
    }

    /// The size in bytes.
    pub fn size(self: Memory) u64 {
        return c.wasm_memory_size_at(self.module.raw(), self.index);
    }

    /// Grows the memory by `delta_pages`, returning the old size in pages, or null when it cannot grow.
    pub fn grow(self: Memory, delta_pages: u64) ?u64 {
        const old = c.wasm_memory_grow_at(self.module.raw(), self.index, delta_pages);
        return if (old < 0) null else @intCast(old);
    }

    pub fn read(self: Memory, offset: u64, dst: []u8) Error!void {
        try check(c.wasm_memory_read(self.module.raw(), self.index, offset, dst.ptr, dst.len));
    }

    pub fn write(self: Memory, offset: u64, src: []const u8) Error!void {
        try check(c.wasm_memory_write(self.module.raw(), self.index, offset, src.ptr, src.len));
    }

    /// Reads a NUL-terminated string at `offset`, truncated to fit `buffer`.
    pub fn readString(self: Memory, offset: u64, buffer: []u8) Error![:0]const u8 {
        try check(c.wasm_memory_get_string(self.module.raw(), self.index, offset, buffer.ptr, buffer.len));
        return span(buffer.ptr);
    }

    pub fn raw(self: Memory) *c.wasm_memory_t {
        return c.wasm_memory_ref_at(self.module.raw(), self.index);
    }
};

/// A memory for the threads proposal, shared by runtimes on separate threads through `Runtime.bindMemory`.
/// It never moves when it grows, and must outlive every module that imports it.
pub const SharedMemory = opaque {
    pub const Options = struct {
        pages: u64,
        max_pages: u64,
        memory64: bool = false,
    };

    pub fn init(options: Options) Error!*SharedMemory {
        const memory = c.wasm_shared_memory_new(options.pages, options.max_pages, @intFromBool(options.memory64));
        return if (memory == null) error.OutOfMemory else @ptrCast(memory);
    }

    pub fn deinit(memory: *SharedMemory) void {
        c.wasm_shared_memory_free(memory.raw());
    }

    pub fn raw(memory: *SharedMemory) *c.wasm_memory_t {
        return @ptrCast(@alignCast(memory));
    }

    /// The bytes currently in bounds. Another thread may grow the memory, which keeps this slice valid.
    pub fn data(memory: *SharedMemory) []u8 {
        const m = memory.raw();
        return m.data[0..@atomicLoad(usize, &m.shared_size, .acquire)];
    }
};
