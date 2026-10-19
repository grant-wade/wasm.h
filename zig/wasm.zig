//! A runtime and its modules belong to one thread at a time.

const std = @import("std");
const native_endian = @import("builtin").cpu.arch.endian();

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
    Interrupted,
    /// A host function returns it to suspend the calling Wasm code until `Runtime.resumeCall`.
    Yield,
    /// The runtime holds a suspended call; resume or cancel it first.
    Suspended,
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
    .{ c.WASM_ERR_INTERRUPTED, error.Interrupted },
    .{ c.WASM_YIELD, error.Yield },
    .{ c.WASM_ERR_SUSPENDED, error.Suspended },
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
            else => if (comptime isExternPointer(T))
                .externref
            else if (comptime ReprInt(T)) |Int|
                (if (@bitSizeOf(Int) <= 32) .i32 else if (@bitSizeOf(Int) <= 64) .i64 else @compileError("no Wasm value type for " ++ @typeName(T)))
            else
                @compileError("no Wasm value type for " ++ @typeName(T)),
        };
    }

    pub fn isRef(self: ValType) bool {
        return switch (self) {
            .i32, .i64, .f32, .f64, .v128, .void => false,
            else => true,
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

/// A function reference, valid across the runtime that produced it; see `Runtime.funcRef`.
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

/// The integer a `bool`, enum, or packed struct converts through, or null for other types.
fn ReprInt(comptime T: type) ?type {
    return switch (@typeInfo(T)) {
        .bool => u1,
        .@"enum" => |e| e.tag_type,
        .@"struct" => |s| s.backing_integer,
        else => null,
    };
}

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
            else => if (comptime ReprInt(T)) |Int| {
                const raw: Int = switch (@typeInfo(T)) {
                    .bool => @intFromBool(x),
                    .@"enum" => @backingInt(x),
                    else => @bitCast(x),
                };
                if (v.kind == .i32) v.of.i32 = widen(i32, raw) else v.of.i64 = widen(i64, raw);
            } else {
                v.of.externref = @intFromPtr(x);
            },
        }
        return v;
    }

    fn widen(comptime Slot: type, raw: anytype) Slot {
        if (@typeInfo(@TypeOf(raw)).int.signedness == .signed) return raw;
        return @bitCast(@as(@Int(.unsigned, @bitSizeOf(Slot)), raw));
    }

    /// `slot` as `Int`, or null when it is out of range.
    fn narrow(comptime Int: type, slot: anytype) ?Int {
        const Slot = @TypeOf(slot);
        if (@typeInfo(Int).int.signedness == .signed) return std.math.cast(Int, slot);
        return std.math.cast(Int, @as(@Int(.unsigned, @bitSizeOf(Slot)), @bitCast(slot)));
    }

    fn toRepr(self: Value, comptime T: type, comptime Int: type) Error!T {
        if (T == bool) return if (self.kind == .i32) self.of.i32 != 0 else self.of.i64 != 0;
        const raw = (if (self.kind == .i32) narrow(Int, self.of.i32) else narrow(Int, self.of.i64)) orelse return error.InvalidConversion;
        return switch (@typeInfo(T)) {
            .@"enum" => |e| if (e.mode == .exhaustive) std.enums.fromInt(T, raw) orelse error.InvalidConversion else @fromBackingInt(raw),
            else => @bitCast(raw),
        };
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
            else => if (comptime ReprInt(T)) |Int| self.toRepr(T, Int) else @ptrFromInt(self.of.externref),
        };
    }

    pub fn format(self: Value, w: *std.Io.Writer) std.Io.Writer.Error!void {
        switch (self.kind) {
            .i32 => try w.print("i32 {d}", .{self.of.i32}),
            .i64 => try w.print("i64 {d}", .{self.of.i64}),
            .f32 => try w.print("f32 {d}", .{self.of.f32}),
            .f64 => try w.print("f64 {d}", .{self.of.f64}),
            .v128 => try w.print("v128 0x{x}", .{&self.of.v128}),
            .funcref, .nullfuncref => if (self.of.funcref == std.math.maxInt(u32))
                try w.print("{t} null", .{self.kind})
            else
                try w.print("{t} {d}", .{ self.kind, self.of.funcref }),
            .i31ref => try w.print("i31ref {d}", .{@as(i32, @bitCast(@as(u32, @truncate(self.of.gc_ref)))) >> 1}),
            else => if (self.of.gc_ref == 0)
                try w.print("{t} null", .{self.kind})
            else
                try w.print("{t} 0x{x}", .{ self.kind, self.of.gc_ref }),
        }
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

/// A reference type: a reference kind such as `.funcref` or `.anyref`, narrowed to a module's
/// type when `type_index` is set.
pub const RefType = struct {
    kind: ValType,
    type_index: ?u32 = null,
    nullable: bool = true,

    fn fromC(kind: ValType, ref: ?*const c.wasm_reftype_t) ?RefType {
        if (!kind.isRef()) return null;
        const r = ref orelse return .{ .kind = kind };
        return .{
            .kind = if (r.type == 0) kind else @fromBackingInt(r.type),
            .type_index = if (r.has_type_index != 0) r.type_index else null,
            .nullable = r.nullable != 0,
        };
    }
};

/// A function signature as the runtime reports it. Slices point into the module.
pub const FuncType = struct {
    params: []const ValType,
    results: []const ValType,
    c_type: *const c.wasm_functype_t,

    fn fromC(ft: ?*const c.wasm_functype_t) ?FuncType {
        const t = ft orelse return null;
        return .{
            .params = valTypes(t.params, t.num_params),
            .results = valTypes(t.results, t.num_results),
            .c_type = t,
        };
    }

    /// Details of reference parameter `i`, or null when it is not a reference.
    pub fn paramRefType(self: FuncType, i: usize) ?RefType {
        return .fromC(self.params[i], if (self.c_type.param_reftypes == null) null else &self.c_type.param_reftypes[i]);
    }

    /// Details of reference result `i`, or null when it is not a reference.
    pub fn resultRefType(self: FuncType, i: usize) ?RefType {
        return .fromC(self.results[i], if (self.c_type.result_reftypes == null) null else &self.c_type.result_reftypes[i]);
    }

    pub fn format(self: FuncType, w: *std.Io.Writer) std.Io.Writer.Error!void {
        try writeValTypes(w, self.params);
        try w.writeAll(" -> ");
        try writeValTypes(w, self.results);
    }

    fn writeValTypes(w: *std.Io.Writer, types: []const ValType) std.Io.Writer.Error!void {
        try w.writeByte('(');
        for (types, 0..) |t, i| {
            if (i > 0) try w.writeAll(", ");
            try w.print("{t}", .{t});
        }
        try w.writeByte(')');
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
    /// Bytes per block of call frame storage; blocks are added as calls nest.
    frame_arena_size: ?usize = null,
    /// Defer validating each function body until its first call.
    lazy_validation: ?bool = null,
    /// Locals per function, parameters included; 0 is unlimited.
    max_function_locals: ?u32 = null,
    /// Bytes per linear memory, at instantiation and on growth; 0 is unlimited.
    max_memory_bytes: ?u64 = null,
    /// Bytes across the linear memories the runtime's modules define; 0 is unlimited.
    max_total_memory_bytes: ?u64 = null,
    /// Elements per table, at instantiation and on growth; 0 is unlimited.
    max_table_elements: ?u64 = null,
    /// Bytes the GC heap may grow to, capping `initial_gc_heap_size`; 0 is unlimited.
    max_gc_heap_bytes: ?u64 = null,
    /// Replaces the default enabled proposals.
    features: ?Features = null,
    /// Sets an initial fuel budget; see `Runtime.setFuel`.
    fuel: ?u64 = null,
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
    /// Details when `type` is a reference type, for global and table imports.
    ref_type: ?RefType,
    mutable: bool,
};

/// How a GC struct field or array element is stored.
pub const StorageType = union(enum) {
    value: ValType,
    i8,
    i16,
};

pub const FieldType = struct {
    storage: StorageType,
    mutable: bool,
    /// Details when the field holds a reference.
    ref_type: ?RefType,

    fn fromC(field: ?*const c.wasm_fieldtype_t) ?FieldType {
        const f = field orelse return null;
        const storage: StorageType = if (f.storage.kind == c.WASM_STORAGE_PACKED)
            (if (f.storage.of.packed_type == c.WASM_PACKED_I8) .i8 else .i16)
        else
            .{ .value = @fromBackingInt(f.storage.of.valtype) };
        return .{
            .storage = storage,
            .mutable = f.is_mutable != 0,
            .ref_type = if (storage == .value) RefType.fromC(storage.value, &f.storage.ref_type) else null,
        };
    }
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
    instance: ?*Instance,
    func_index: u32,
    offset: u32,

    /// The function's name from its module's "name" section.
    pub fn funcName(frame: CallFrame) ?[:0]const u8 {
        const instance = frame.instance orelse return null;
        return instance.info().funcName(frame.func_index);
    }

    /// Writes the frame as the C backtrace does, such as `app!parse (func[12]) at offset 0x002A`.
    pub fn format(frame: CallFrame, w: *std.Io.Writer) std.Io.Writer.Error!void {
        if (frame.instance) |instance| {
            if (instance.info().name()) |module_name| try w.print("{s}!", .{module_name});
        }
        if (frame.funcName()) |func_name| {
            try w.print("{s} (func[{d}])", .{ func_name, frame.func_index });
        } else {
            try w.print("func[{d}]", .{frame.func_index});
        }
        try w.print(" at offset 0x{X:0>4}", .{frame.offset});
    }
};

fn span(ptr: [*c]const u8) [:0]const u8 {
    if (ptr == null) return "";
    return std.mem.span(@as([*:0]const u8, @ptrCast(ptr)));
}

/// Serves a `wasm_allocator_t` from the `std.mem.Allocator` its ctx points to.
const AllocatorBridge = struct {
    fn from(ctx: ?*anyopaque) std.mem.Allocator {
        return @as(*const std.mem.Allocator, @ptrCast(@alignCast(ctx))).*;
    }

    fn alloc(ctx: ?*anyopaque, size: usize, alignment: usize) callconv(.c) ?*anyopaque {
        return from(ctx).rawAlloc(size, .fromByteUnits(alignment), @returnAddress());
    }

    fn resize(ctx: ?*anyopaque, p: ?*anyopaque, old_size: usize, new_size: usize, alignment: usize) callconv(.c) ?*anyopaque {
        const a = from(ctx);
        const al: std.mem.Alignment = .fromByteUnits(alignment);
        const old = @as([*]u8, @ptrCast(p))[0..old_size];
        if (a.rawRemap(old, al, new_size, @returnAddress())) |moved| return moved;
        const moved = a.rawAlloc(new_size, al, @returnAddress()) orelse return null;
        const keep = @min(old_size, new_size);
        @memcpy(moved[0..keep], old[0..keep]);
        a.rawFree(old, al, @returnAddress());
        return moved;
    }

    fn free(ctx: ?*anyopaque, p: ?*anyopaque, size: usize, alignment: usize) callconv(.c) void {
        from(ctx).rawFree(@as([*]u8, @ptrCast(p))[0..size], .fromByteUnits(alignment), @returnAddress());
    }

    /// The returned allocator's ctx is `stored`, which must outlive every block it allocates.
    fn wrap(stored: *const std.mem.Allocator) c.wasm_allocator_t {
        return .{ .ctx = @constCast(stored), .alloc = alloc, .resize = resize, .free = free };
    }

    fn box(allocator: std.mem.Allocator) Error!*std.mem.Allocator {
        const b = allocator.create(std.mem.Allocator) catch return error.OutOfMemory;
        b.* = allocator;
        return b;
    }

    fn unbox(ctx: ?*anyopaque) void {
        const b: *std.mem.Allocator = @ptrCast(@alignCast(ctx));
        b.*.destroy(b);
    }
};

pub const Runtime = opaque {
    /// The runtime allocates everything it uses from `allocator`, which must outlive it.
    pub fn init(allocator: std.mem.Allocator, config: Config) Error!*Runtime {
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
        if (config.max_total_memory_bytes) |v| cfg.max_total_memory_bytes = v;
        if (config.max_table_elements) |v| cfg.max_table_elements = v;
        if (config.max_gc_heap_bytes) |v| cfg.max_gc_heap_bytes = v;
        if (cfg.max_stack_values == 0 or cfg.max_call_depth == 0 or cfg.max_labels == 0) return error.Malformed;
        const b = try AllocatorBridge.box(allocator);
        const bridged = AllocatorBridge.wrap(b);
        const rt: *Runtime = @ptrCast(c.wasm_runtime_new(&bridged, &cfg) orelse {
            allocator.destroy(b);
            return error.OutOfMemory;
        });
        if (config.features) |set| rt.setFeatures(set);
        if (config.fuel) |units| rt.setFuel(units);
        return rt;
    }

    /// Free its modules first.
    pub fn deinit(rt: *Runtime) void {
        const ctx = c.wasm_runtime_allocator(rt.raw()).*.ctx;
        c.wasm_runtime_free(rt.raw());
        AllocatorBridge.unbox(ctx);
    }

    pub fn raw(rt: *Runtime) *c.wasm_runtime_t {
        return @ptrCast(rt);
    }

    /// Describes the most recent error; "ok" when there is none.
    pub fn errorMessage(rt: *Runtime) [:0]const u8 {
        return span(c.wasm_runtime_error_message(rt.raw()));
    }

    /// The most recent error, or null when there is none.
    pub fn lastError(rt: *Runtime) ?Error {
        check(c.wasm_runtime_last_error(rt.raw())) catch |err| return err;
        return null;
    }

    pub fn clearError(rt: *Runtime) void {
        c.wasm_runtime_clear_error(rt.raw());
    }

    fn fail(rt: *Runtime, err: Error, message: [:0]const u8) Error {
        c.wasm_runtime_set_error(rt.raw(), errorCode(err), message);
        return err;
    }

    pub fn features(rt: *Runtime) Features {
        return @bitCast(c.wasm_runtime_enabled_features(rt.raw()));
    }

    /// Replaces the enabled feature set. Applies to modules loaded afterwards.
    pub fn setFeatures(rt: *Runtime, set: Features) void {
        c.wasm_runtime_set_enabled_features(rt.raw(), set.bits());
    }

    pub fn enableFeatures(rt: *Runtime, set: Features) void {
        c.wasm_runtime_enable_feature(rt.raw(), set.bits());
    }

    pub fn disableFeatures(rt: *Runtime, set: Features) void {
        c.wasm_runtime_disable_feature(rt.raw(), set.bits());
    }

    pub fn enableAllFeatures(rt: *Runtime) void {
        c.wasm_runtime_enable_all_features(rt.raw());
    }

    fn lastErrorOr(rt: *Runtime, fallback: Error) Error {
        return rt.lastError() orelse fallback;
    }

    /// `bytes` is copied and may be freed afterwards.
    pub fn load(rt: *Runtime, bytes: []const u8) Error!*Instance {
        return @ptrCast(c.wasm_load(rt.raw(), bytes.ptr, bytes.len) orelse return rt.lastErrorOr(error.Malformed));
    }

    pub fn compile(rt: *Runtime, bytes: []const u8) Error!*CompiledModule {
        return @ptrCast(c.wasm_compile(rt.raw(), bytes.ptr, bytes.len) orelse return rt.lastErrorOr(error.Malformed));
    }

    /// `compiled` may come from another runtime, including one on another thread.
    pub fn instantiate(rt: *Runtime, compiled: *const CompiledModule) Error!*Instance {
        return rt.instantiateRaw(compiled, null);
    }

    /// `context` (a single-item pointer) becomes the instance's userdata before its start function
    /// runs, and functions bound with `bindInstanceFunc` receive it.
    pub fn instantiateContext(rt: *Runtime, compiled: *const CompiledModule, context: anytype) Error!*Instance {
        comptime requireSinglePointer(@TypeOf(context), "context");
        return rt.instantiateRaw(compiled, @ptrCast(@constCast(context)));
    }

    fn instantiateRaw(rt: *Runtime, compiled: *const CompiledModule, userdata: ?*anyopaque) Error!*Instance {
        return @ptrCast(c.wasm_instantiate(rt.raw(), compiled.raw(), userdata) orelse return rt.lastErrorOr(error.Malformed));
    }

    pub fn checkImport(rt: *Runtime, compiled: *const CompiledModule, import_index: u32) Error!void {
        try check(c.wasm_import_check(rt.raw(), compiled.raw(), import_index));
    }

    /// Iterates the imports of `compiled` that this runtime's bindings do not resolve.
    pub fn unresolvedImports(rt: *Runtime, compiled: *const CompiledModule) UnresolvedImports {
        return .{ .runtime = rt, .compiled = compiled };
    }

    /// `instance` must belong to this runtime and outlive the modules that import from it.
    pub fn registerModule(rt: *Runtime, module_name: [:0]const u8, instance: *Instance) Error!void {
        try check(c.wasm_register_module(rt.raw(), module_name.ptr, instance.raw()));
    }

    /// Binds a Zig function as the import `module_name.name` for modules loaded afterwards.
    ///
    /// `func` may take a leading `*Instance` (the caller) followed by Wasm-typed parameters, and may return
    /// `void`, one Wasm-typed value, a tuple of them, or an error union of those. A returned error traps with
    /// its name as the message: members of `Error` keep their code, anything else becomes `error.Trap`.
    pub fn bindFunc(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, comptime func: anytype) Error!void {
        return rt.bindHost(module_name, name, null, HostFunc(.none, void, func));
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
        comptime requireSinglePointer(Context, "context");
        return rt.bindHost(module_name, name, @ptrCast(@constCast(context)), HostFunc(.binding, Context, func));
    }

    /// Like `bindFunc`, with the calling instance's context (see `instantiateContext`) passed as
    /// `func`'s first parameter, whose type must match it. A call from an instance without a
    /// context traps.
    pub fn bindInstanceFunc(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, comptime func: anytype) Error!void {
        const params = @typeInfo(@TypeOf(func)).@"fn".param_types;
        if (params.len == 0) @compileError("the first parameter of an instance function must be its context pointer");
        const Context = params[0] orelse @compileError("anytype parameters are not supported");
        comptime requireSinglePointer(Context, "an instance function's first parameter");
        return rt.bindHost(module_name, name, null, HostFunc(.instance, Context, func));
    }

    /// Binds each public function of `Host` as the import `module_name.<function name>`, as `bindFunc` does.
    pub fn bindAll(rt: *Runtime, module_name: [:0]const u8, comptime Host: type) Error!void {
        inline for (comptime hostFuncNames(Host)) |name| try rt.bindFunc(module_name, name, @field(Host, name));
    }

    /// Like `bindAll`, binding the functions whose first parameter has `context`'s type as
    /// `bindFuncContext` does.
    pub fn bindAllContext(rt: *Runtime, module_name: [:0]const u8, comptime Host: type, context: anytype) Error!void {
        const Context = @TypeOf(context);
        inline for (comptime hostFuncNames(Host)) |name| {
            const func = @field(Host, name);
            const params = @typeInfo(@TypeOf(func)).@"fn".param_types;
            if (params.len > 0 and params[0] == Context) {
                try rt.bindFuncContext(module_name, name, context, func);
            } else {
                try rt.bindFunc(module_name, name, func);
            }
        }
    }

    /// `func` takes `context` (unless it is `{}`), the calling instance, the arguments, and a slice
    /// for the results, and may return an error as for `bindFunc`.
    pub fn bindFuncDynamic(
        rt: *Runtime,
        module_name: [:0]const u8,
        name: [:0]const u8,
        params: []const ValType,
        results: []const ValType,
        context: anytype,
        comptime func: anytype,
    ) Error!void {
        const Context = @TypeOf(context);
        if (Context != void) comptime requireSinglePointer(Context, "context");
        var imp = std.mem.zeroes(c.wasm_import_t);
        imp.module = module_name.ptr;
        imp.name = name.ptr;
        imp.type = cFuncType(params, results);
        imp.func = DynamicFunc(Context, func).call;
        imp.userdata = if (Context == void) null else @ptrCast(@constCast(context));
        try check(c.wasm_register_import(rt.raw(), &imp));
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
    /// `value` must outlive every module that imports it, which read and write it in place, and
    /// must not hold a host anyref.
    pub fn bindGlobal(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, value: *Value, mutable: bool) Error!void {
        var imp = std.mem.zeroes(c.wasm_global_import_t);
        imp.module = module_name.ptr;
        imp.name = name.ptr;
        imp.type = @backingInt(value.kind);
        imp.is_mutable = @intFromBool(mutable);
        imp.value = @ptrCast(value);
        try check(c.wasm_register_global_import(rt.raw(), &imp));
    }

    /// `memory` is a `*HostMemory`, `*SharedMemory`, an instance's `Memory`, or a `*c.wasm_memory_t`.
    pub fn bindMemory(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, memory: anytype) Error!void {
        const handle: *c.wasm_memory_t = if (@TypeOf(memory) == *c.wasm_memory_t) memory else memory.raw();
        const imp: c.wasm_memory_import_t = .{ .module = module_name.ptr, .name = name.ptr, .memory = handle };
        try check(c.wasm_register_memory_import(rt.raw(), &imp));
    }

    /// `table` is a `*HostTable`, an instance's `Table`, or a `*c.wasm_table_t`.
    pub fn bindTable(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, table: anytype) Error!void {
        const handle: *c.wasm_table_t = if (@TypeOf(table) == *c.wasm_table_t) table else table.raw();
        const imp: c.wasm_table_import_t = .{ .module = module_name.ptr, .name = name.ptr, .table = handle };
        try check(c.wasm_register_table_import(rt.raw(), &imp));
    }

    /// Binds a new tag whose payload has `Fn`'s parameter types, such as `fn (i32, f64) void`.
    pub fn bindTag(rt: *Runtime, module_name: [:0]const u8, name: [:0]const u8, comptime Fn: type) Error!void {
        const sig = Signature(Fn);
        if (sig.results.len != 0) @compileError("tag types have no results");
        const params = comptime valTypesOf(sig.params);
        var imp = std.mem.zeroes(c.wasm_tag_import_t);
        imp.module = module_name.ptr;
        imp.name = name.ptr;
        imp.type = cFuncType(&params, &.{});
        try check(c.wasm_register_tag_import(rt.raw(), &imp));
    }

    /// Binds the partial `wasi_snapshot_preview1` imports. Needs the `platform` build option.
    pub fn bindWasi(rt: *Runtime) Error!void {
        try check(c.wasm_bind_wasi_stubs(rt.raw()));
    }

    /// Sets the strings the guest reads through WASI `args_get`, starting with its program name.
    /// Needs the `platform` build option; the strings are copied.
    pub fn setWasiArgs(rt: *Runtime, args: []const [*:0]const u8) Error!void {
        try check(c.wasm_wasi_set_args(rt.raw(), @intCast(args.len), @ptrCast(args.ptr)));
    }

    /// Sets the `NAME=value` strings the guest reads through WASI `environ_get`. Needs the
    /// `platform` build option; the strings are copied.
    pub fn setWasiEnv(rt: *Runtime, env: []const [*:0]const u8) Error!void {
        try check(c.wasm_wasi_set_env(rt.raw(), @intCast(env.len), @ptrCast(env.ptr)));
    }

    /// Lets the guest open files beneath the host directory `host_path`, which it sees as
    /// `guest_path`. Needs the `platform` build option.
    pub fn preopenDir(rt: *Runtime, host_path: [*:0]const u8, guest_path: [*:0]const u8) Error!void {
        try check(c.wasm_wasi_preopen_dir(rt.raw(), host_path, guest_path));
    }

    /// Binds Emscripten's `env` imports, some of which reach the host filesystem: use only with trusted
    /// modules. Needs the `platform` build option.
    pub fn bindEmscripten(rt: *Runtime) Error!void {
        try check(c.wasm_bind_emscripten_stubs(rt.raw()));
    }

    pub fn callRef(rt: *Runtime, ref: FuncRef, args: []const Value, results: []Value) Error!void {
        try check(c.wasm_call_ref(rt.raw(), Value.from(ref).toC(), Value.cPtr(args), @intCast(args.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    /// The function `ref` refers to, checked against `Fn` as `Instance.func` does.
    pub fn funcRef(rt: *Runtime, ref: FuncRef, comptime Fn: type) Error!TypedFunc(Fn) {
        var mod: ?*c.wasm_module_t = null;
        var index: u32 = 0;
        check(c.wasm_func_ref_target(rt.raw(), Value.from(ref).toC(), &mod, &index)) catch |err|
            return rt.fail(err, "funcref does not refer to a live function");
        const instance: *Instance = @ptrCast(mod.?);
        return instance.funcAt(index, Fn);
    }

    pub fn collectGarbage(rt: *Runtime) void {
        c.wasm_gc_collect(rt.raw());
    }

    /// Keeps `value` alive across collections until `Held.release`.
    pub fn hold(rt: *Runtime, value: Value) Error!Held {
        const slot = c.wasm_ref_hold(rt.raw(), value.toC()) orelse return error.OutOfMemory;
        return .{ .runtime = rt, .slot = @ptrCast(slot) };
    }

    /// Limits execution to `fuel` units; a call that runs out stops with `error.OutOfFuel`.
    pub fn setFuel(rt: *Runtime, fuel_units: u64) void {
        c.wasm_runtime_set_fuel(rt.raw(), fuel_units);
    }

    /// Makes the running call stop with `error.Interrupted` at its next loop iteration, function
    /// entry, atomic wait, or WASI sleep, or the next call fail before it starts. Safe to call from
    /// any thread.
    pub fn interrupt(rt: *Runtime) void {
        c.wasm_runtime_interrupt(rt.raw());
    }

    /// Stops metering until `setFuel` is called again.
    pub fn disableFuel(rt: *Runtime) void {
        c.wasm_runtime_disable_fuel(rt.raw());
    }

    pub fn fuel(rt: *Runtime) u64 {
        return c.wasm_runtime_fuel(rt.raw());
    }

    /// Why a call is suspended.
    pub const Suspension = enum { out_of_fuel, interrupted, yield };

    /// Why the last call is suspended, or null if none is.
    pub fn suspended(rt: *Runtime) ?Suspension {
        return switch (c.wasm_runtime_suspended(rt.raw())) {
            c.WASM_ERR_OUT_OF_FUEL => .out_of_fuel,
            c.WASM_ERR_INTERRUPTED => .interrupted,
            c.WASM_YIELD => .yield,
            else => null,
        };
    }

    /// Continues the suspended call. `host_results` are the yielding host function's results and
    /// must be empty after a fuel or interrupt stop; `results` receives the call's results.
    pub fn resumeCall(rt: *Runtime, host_results: []const Value, results: []Value) Error!void {
        try check(c.wasm_runtime_resume(rt.raw(), Value.cPtr(host_results), @intCast(host_results.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    /// Discards the suspended call, if any.
    pub fn cancel(rt: *Runtime) void {
        c.wasm_runtime_cancel(rt.raw());
    }

    /// The status the guest passed to WASI `proc_exit` when a call failed with `error.Exit`.
    pub fn exitCode(rt: *Runtime) i32 {
        return c.wasm_runtime_exit_code(rt.raw());
    }

    /// Writes the current Wasm call stack into `buffer`, typically from a host function.
    pub fn backtrace(rt: *Runtime, buffer: []u8) [:0]const u8 {
        if (buffer.len == 0) return "";
        c.wasm_runtime_backtrace(rt.raw(), buffer.ptr, buffer.len);
        return span(buffer.ptr);
    }

    pub fn callDepth(rt: *Runtime) u32 {
        return c.wasm_runtime_call_depth(rt.raw());
    }

    /// Iterates the frames of the running or last trapped call, innermost first.
    pub fn frames(rt: *Runtime) Frames {
        return .{ .runtime = rt };
    }

    pub fn callFrame(rt: *Runtime, depth: u32) ?CallFrame {
        var frame: CallFrame = undefined;
        var mod: ?*c.wasm_module_t = null;
        check(c.wasm_runtime_call_frame(rt.raw(), depth, &mod, &frame.func_index, &frame.offset)) catch return null;
        frame.instance = @ptrCast(mod);
        return frame;
    }
};

pub const Frames = struct {
    runtime: *Runtime,
    depth: u32 = 0,

    pub fn next(self: *Frames) ?CallFrame {
        const frame = self.runtime.callFrame(self.depth) orelse return null;
        self.depth += 1;
        return frame;
    }
};

/// A reference kept alive across collections, which update it when they move the object.
pub const Held = struct {
    runtime: *Runtime,
    /// Do not store a host anyref here.
    slot: *Value,

    pub fn get(self: Held) Value {
        return self.slot.*;
    }

    pub fn release(self: Held) void {
        c.wasm_ref_release(self.runtime.raw(), @ptrCast(self.slot));
    }
};

const empty_valtypes = [1]c.wasm_valtype_t{0};

/// A C signature over `params` and `results`, which must outlive it. Its arrays are never null.
fn cFuncType(params: []const ValType, results: []const ValType) c.wasm_functype_t {
    var ft = std.mem.zeroes(c.wasm_functype_t);
    ft.num_params = @intCast(params.len);
    ft.num_results = @intCast(results.len);
    ft.params = @constCast(cValTypes(params));
    ft.results = @constCast(cValTypes(results));
    return ft;
}

fn cValTypes(types: []const ValType) [*]const c.wasm_valtype_t {
    return if (types.len == 0) &empty_valtypes else @ptrCast(types.ptr);
}

fn valTypesOf(comptime types: []const type) [types.len]ValType {
    var out: [types.len]ValType = undefined;
    for (types, 0..) |T, i| out[i] = ValType.of(T) orelse @compileError("tags need concrete Wasm types, not Value");
    return out;
}

/// An import of a compiled module that a runtime's bindings do not resolve.
pub const ImportProblem = struct {
    import: Import,
    err: Error,
    /// The runtime's error message, valid until its next error.
    message: [:0]const u8,
};

pub const UnresolvedImports = struct {
    runtime: *Runtime,
    compiled: *const CompiledModule,
    index: u32 = 0,

    pub fn next(self: *UnresolvedImports) ?ImportProblem {
        const info = self.compiled.info();
        while (self.index < info.importCount()) {
            const i = self.index;
            self.index += 1;
            self.runtime.checkImport(self.compiled, i) catch |err| return .{
                .import = info.importAt(i),
                .err = err,
                .message = self.runtime.errorMessage(),
            };
        }
        return null;
    }
};

fn requireSinglePointer(comptime T: type, comptime what: []const u8) void {
    if (@typeInfo(T) != .pointer or @typeInfo(T).pointer.size != .one)
        @compileError(what ++ " must be a single-item pointer, found " ++ @typeName(T));
}

fn hostFuncNames(comptime Host: type) []const [:0]const u8 {
    var names: []const [:0]const u8 = &.{};
    for (@typeInfo(Host).@"struct".decl_names) |name| {
        if (@typeInfo(@TypeOf(@field(Host, name))) == .@"fn") names = names ++ .{name};
    }
    return names;
}

/// Fails to compile unless `Host` has a function with the Wasm signature of each function type
/// `Imports` declares, ignoring leading context and `*Instance` parameters.
pub fn checkHost(comptime Imports: type, comptime Host: type) void {
    for (@typeInfo(Imports).@"struct".decl_names) |name| {
        const Expected = @field(Imports, name);
        if (@TypeOf(Expected) != type or @typeInfo(Expected) != .@"fn") continue;
        if (!@hasDecl(Host, name)) @compileError(@typeName(Host) ++ " has no function for import " ++ name);
        const Actual = @TypeOf(@field(Host, name));
        const expected = Signature(Expected);
        const actual = Signature(Actual);
        var first: usize = 0;
        while (first < actual.params.len and @typeInfo(actual.params[first]) == .pointer) first += 1;
        if (!sameWasmTypes(expected.params, guestExpanded(actual.params[first..])) or !sameWasmTypes(expected.results, actual.results))
            @compileError(@typeName(Host) ++ "." ++ name ++ " does not match import type " ++ @typeName(Expected));
    }
}

/// Fails to compile unless each field of `Spec` (see `Bound`) names an export of `Generated`, the
/// `Exports` struct wasm2zig writes, with the same kind and Wasm types. A `Value` matches any type,
/// and an optional field may name a missing export.
pub fn checkExports(comptime Generated: type, comptime Spec: type) void {
    const fields = @typeInfo(Spec).@"struct";
    for (fields.field_names, fields.field_types) |name, T| {
        const B = BoundField(T);
        const S = if (@typeInfo(B) == .optional) @typeInfo(B).optional.child else B;
        if (!@hasField(Generated, name)) {
            if (S != B) continue;
            @compileError(@typeName(Generated) ++ " has no export " ++ name);
        }
        const G = BoundField(@FieldType(Generated, name));
        if (!exportMatches(S, G))
            @compileError("export " ++ name ++ " has type " ++ @typeName(@FieldType(Generated, name)) ++ ", not " ++ @typeName(T));
    }
}

fn exportMatches(comptime S: type, comptime G: type) bool {
    if (S.extern_kind != G.extern_kind) return false;
    return switch (S.extern_kind) {
        .func => matchesWasmTypes(Signature(S.FnType).params, Signature(G.FnType).params) and
            matchesWasmTypes(Signature(S.FnType).results, Signature(G.FnType).results),
        .global => matchesWasmTypes(&.{S.ValueType}, &.{G.ValueType}),
        else => true,
    };
}

/// Whether each of `spec` has the Wasm type of its counterpart in `generated`; `Value` matches any.
fn matchesWasmTypes(comptime spec: []const type, comptime generated: []const type) bool {
    if (spec.len != generated.len) return false;
    for (spec, generated) |s, g| {
        const expected = ValType.of(s) orelse continue;
        if (expected != ValType.of(g)) return false;
    }
    return true;
}

/// The function type of a `Spec` field: a function type, `TypedFunc`, or an optional of either.
fn SpecFn(comptime T: type) type {
    return switch (@typeInfo(T)) {
        .optional => |o| SpecFn(o.child),
        .@"fn" => T,
        else => if (@hasDecl(T, "FnType")) T.FnType else @compileError(@typeName(T) ++ " is not a function export"),
    };
}

/// The type of parameter `i` of `Spec`'s function export `name`.
pub fn ExportParam(comptime Spec: type, comptime name: []const u8, comptime i: usize) type {
    return Signature(SpecFn(@FieldType(Spec, name))).params[i];
}

/// The result of `Spec`'s function export `name`: `void`, a value, or a tuple.
pub fn ExportResult(comptime Spec: type, comptime name: []const u8) type {
    return Signature(SpecFn(@FieldType(Spec, name))).Result;
}

/// Calls the function export `name` of `exports`, a `Bound(Spec)`. A missing optional export fails
/// with `error.UndefinedExport`.
pub fn callExport(exports: anytype, comptime name: []const u8, args: anytype) Error!ExportResult(@TypeOf(exports), name) {
    const field = @field(exports, name);
    const func = if (@typeInfo(@TypeOf(field)) == .optional) field orelse return error.UndefinedExport else field;
    return func.call(args);
}

fn sameWasmTypes(comptime expected: []const type, comptime actual: []const type) bool {
    if (expected.len != actual.len) return false;
    for (expected, actual) |e, a| {
        if (ValType.of(e) != ValType.of(a)) return false;
    }
    return true;
}

fn isGuestParam(comptime T: type) bool {
    return @typeInfo(T) == .@"struct" and @hasDecl(T, "guest_params");
}

/// `params` with each guest parameter type replaced by the Wasm parameters it takes.
fn guestExpanded(comptime params: []const type) []const type {
    var out: []const type = &.{};
    for (params) |T| out = out ++ if (isGuestParam(T)) &T.guest_params else &[_]type{T};
    return out;
}

fn hostFail(mod: ?*c.wasm_module_t, err: anyerror) c.wasm_error_t {
    const code = errorCode(err);
    c.wasm_runtime_set_error(c.wasm_module_runtime(mod), code, @errorName(err));
    return code;
}

const ContextSource = enum { none, binding, instance };

/// The C callback for a bound Zig function. `Context` is `void` when `source` is `.none`.
fn HostFunc(comptime source: ContextSource, comptime Context: type, comptime func: anytype) type {
    const Fn = @TypeOf(func);
    const sig = Signature(Fn);
    const context_count: usize = @intFromBool(source != .none);
    if (context_count == 1 and (sig.params.len == 0 or sig.params[0] != Context))
        @compileError("the first parameter of a bound function must be the context type " ++ @typeName(Context));
    const Caller = if (sig.params.len > context_count) sig.params[context_count] else void;
    const has_caller = Caller == *Instance or Caller == *Runtime;
    const first = context_count + @intFromBool(has_caller);
    const zig_params = sig.params[first..];
    const wasm_params = guestExpanded(zig_params);

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
            const instance: *Instance = @ptrCast(mod.?);
            var call_args: std.meta.ArgsTuple(Fn) = undefined;
            switch (source) {
                .none => {},
                .binding => call_args[0] = @ptrCast(@alignCast(userdata.?)),
                .instance => call_args[0] = @ptrCast(@alignCast(c.wasm_module_userdata(mod) orelse {
                    c.wasm_runtime_set_error(c.wasm_module_runtime(mod), c.WASM_ERR_TRAP, "the calling instance has no context");
                    return c.WASM_ERR_TRAP;
                })),
            }
            if (Caller == *Instance) call_args[context_count] = instance;
            if (Caller == *Runtime) call_args[context_count] = instance.runtime();
            const values: []const Value = if (num_args == 0) &.{} else @as([*]const Value, @ptrCast(args))[0..num_args];
            comptime var arg: usize = 0;
            inline for (zig_params, 0..) |T, i| {
                if (comptime isGuestParam(T)) {
                    call_args[first + i] = T.fromGuest(instance, values[arg..][0..T.guest_params.len]) catch |err| return hostFail(mod, err);
                    arg += T.guest_params.len;
                } else {
                    call_args[first + i] = values[arg].to(T) catch |err| return hostFail(mod, err);
                    arg += 1;
                }
            }
            const returned = @call(.auto, func, call_args);
            const result = if (sig.returns_error) returned catch |err| return hostFail(mod, err) else returned;
            if (sig.results.len > 0) {
                const out_values: [*]Value = @ptrCast(out);
                sig.packResults(result, out_values[0..sig.results.len]);
            }
            return c.WASM_OK;
        }
    };
}

/// The C callback for `Runtime.bindFuncDynamic`.
fn DynamicFunc(comptime Context: type, comptime func: anytype) type {
    return struct {
        fn call(
            mod: ?*c.wasm_module_t,
            args: [*c]const c.wasm_value_t,
            num_args: u32,
            out: [*c]c.wasm_value_t,
            num_results: u32,
            userdata: ?*anyopaque,
        ) callconv(.c) c.wasm_error_t {
            const instance: *Instance = @ptrCast(mod.?);
            const in: []const Value = if (num_args == 0) &.{} else @as([*]const Value, @ptrCast(args))[0..num_args];
            const results: []Value = if (num_results == 0) &.{} else @as([*]Value, @ptrCast(out))[0..num_results];
            const returned = if (Context == void)
                func(instance, in, results)
            else
                func(@as(Context, @ptrCast(@alignCast(userdata.?))), instance, in, results);
            if (@typeInfo(@TypeOf(returned)) == .error_union) returned catch |err| return hostFail(mod, err);
            return c.WASM_OK;
        }
    };
}

/// An exported function with a signature checked against `Fn`, e.g. `fn (i32, i32) i32`.
pub fn TypedFunc(comptime Fn: type) type {
    const sig = Signature(Fn);
    return struct {
        instance: *Instance,
        index: u32,

        pub const extern_kind = ExternKind.func;
        pub const FnType = Fn;

        const Self = @This();

        pub fn call(self: Self, args: std.meta.ArgsTuple(Fn)) Error!sig.Result {
            var params: [sig.params.len]Value = undefined;
            inline for (0..sig.params.len) |i| params[i] = .from(args[i]);
            var results: [sig.results.len]Value = undefined;
            try self.instance.callIndex(self.index, &params, &results);
            return sig.unpackResults(&results);
        }

        /// Continues this function's suspended call; see `Runtime.resumeCall`.
        pub fn resumeCall(self: Self, host_results: []const Value) Error!sig.Result {
            var results: [sig.results.len]Value = undefined;
            try self.instance.runtime().resumeCall(host_results, &results);
            return sig.unpackResults(&results);
        }

        /// How a call that may suspend ended.
        pub const Outcome = union(enum) {
            done: sig.Result,
            suspended: Runtime.Suspension,
        };

        /// Returns `.suspended` when the call suspends.
        pub fn start(self: Self, args: std.meta.ArgsTuple(Fn)) Error!Outcome {
            return self.outcome(self.call(args));
        }

        /// Continues the suspended call with a tuple of the yielding host function's results,
        /// empty after a fuel or interrupt stop.
        pub fn resumeWith(self: Self, host_results: anytype) Error!Outcome {
            var values: [host_results.len]Value = undefined;
            inline for (0..host_results.len) |i| values[i] = .from(host_results[i]);
            return self.outcome(self.resumeCall(&values));
        }

        fn outcome(self: Self, result: Error!sig.Result) Error!Outcome {
            if (result) |value| return .{ .done = value } else |err| switch (err) {
                error.OutOfFuel, error.Interrupted, error.Yield => {
                    const why = self.instance.runtime().suspended() orelse return err;
                    return .{ .suspended = why };
                },
                else => return err,
            }
        }
    };
}

/// The imports, exports, types, and custom sections of a compiled module or instance.
pub const ModuleInfo = opaque {
    pub fn raw(info: *const ModuleInfo) *c.wasm_module_t {
        return @ptrCast(@constCast(info));
    }

    pub fn runtime(info: *const ModuleInfo) *Runtime {
        return @ptrCast(c.wasm_module_runtime(info.raw()));
    }

    /// The module's name from its "name" section.
    pub fn name(info: *const ModuleInfo) ?[:0]const u8 {
        const ptr = c.wasm_module_name(info.raw()) orelse return null;
        return span(ptr);
    }

    pub fn exportCount(info: *const ModuleInfo) u32 {
        return c.wasm_export_count(info.raw());
    }

    pub fn exportAt(info: *const ModuleInfo, export_index: u32) Export {
        var len: usize = 0;
        const export_name = c.wasm_export_name_bytes(info.raw(), export_index, &len);
        return .{
            .name = if (export_name == null) "" else export_name[0..len],
            .kind = @fromBackingInt(c.wasm_export_kind(info.raw(), export_index)),
            .index = c.wasm_export_index(info.raw(), export_index),
        };
    }

    pub fn findExport(info: *const ModuleInfo, export_name: []const u8) ?Export {
        var kind: c.wasm_export_kind_t = undefined;
        var index: u32 = undefined;
        if (c.wasm_find_export_bytes(info.raw(), export_name.ptr, export_name.len, &kind, &index) == 0) return null;
        return .{ .name = export_name, .kind = @fromBackingInt(kind), .index = index };
    }

    pub fn exports(info: *const ModuleInfo) Iterator(Export, exportCount, exportAt) {
        return .{ .info = info };
    }

    pub fn importCount(info: *const ModuleInfo) u32 {
        return c.wasm_import_count(info.raw());
    }

    pub fn importAt(info: *const ModuleInfo, import_index: u32) Import {
        const imp = c.wasm_import_info(info.raw(), import_index).*;
        return .{
            .module = span(imp.module),
            .name = span(imp.name),
            .kind = @fromBackingInt(@intCast(imp.kind)),
            .index = imp.index,
            .type_index = imp.type_index,
            .type = @fromBackingInt(imp.type),
            .ref_type = if (imp.kind == c.WASM_IMPORT_GLOBAL or imp.kind == c.WASM_IMPORT_TABLE) RefType.fromC(@fromBackingInt(imp.type), &imp.ref_type) else null,
            .mutable = imp.is_mutable != 0,
        };
    }

    pub fn imports(info: *const ModuleInfo) Iterator(Import, importCount, importAt) {
        return .{ .info = info };
    }

    pub fn customSectionCount(info: *const ModuleInfo) u32 {
        return c.wasm_custom_section_count(info.raw());
    }

    pub fn customSectionAt(info: *const ModuleInfo, section_index: u32) CustomSection {
        var len: usize = 0;
        const data = c.wasm_custom_section_data(info.raw(), section_index, &len);
        return .{
            .name = span(c.wasm_custom_section_name(info.raw(), section_index)),
            .data = if (data == null) &.{} else data[0..len],
        };
    }

    pub fn customSections(info: *const ModuleInfo) Iterator(CustomSection, customSectionCount, customSectionAt) {
        return .{ .info = info };
    }

    /// The contents of the first custom section called `section_name`.
    pub fn customSection(info: *const ModuleInfo, section_name: [:0]const u8) ?[]const u8 {
        var len: usize = 0;
        const data = c.wasm_custom_section_find(info.raw(), section_name.ptr, &len);
        return if (data == null) null else data[0..len];
    }

    pub fn funcCount(info: *const ModuleInfo) u32 {
        return c.wasm_func_count(info.raw());
    }

    pub fn funcType(info: *const ModuleInfo, func_index: u32) ?FuncType {
        return .fromC(c.wasm_func_functype(info.raw(), func_index));
    }

    /// The function's name from the module's "name" section.
    pub fn funcName(info: *const ModuleInfo, func_index: u32) ?[:0]const u8 {
        const ptr = c.wasm_func_name(info.raw(), func_index) orelse return null;
        return span(ptr);
    }

    pub fn typeCount(info: *const ModuleInfo) u32 {
        return c.wasm_type_count(info.raw());
    }

    pub fn typeKind(info: *const ModuleInfo, type_index: u32) TypeKind {
        return @fromBackingInt(c.wasm_type_kind(info.raw(), type_index));
    }

    /// The signature of function type `type_index`, or null for struct and array types.
    pub fn typeFunc(info: *const ModuleInfo, type_index: u32) ?FuncType {
        return .fromC(c.wasm_type_functype(info.raw(), type_index));
    }

    pub fn structFieldCount(info: *const ModuleInfo, type_index: u32) u32 {
        return c.wasm_struct_type_field_count(info.raw(), type_index);
    }

    /// Field `field_index` of struct type `type_index`, or null when there is none.
    pub fn structField(info: *const ModuleInfo, type_index: u32, field_index: u32) ?FieldType {
        return .fromC(c.wasm_struct_type_field(info.raw(), type_index, field_index));
    }

    /// The element type of array type `type_index`, or null when it is not an array type.
    pub fn arrayField(info: *const ModuleInfo, type_index: u32) ?FieldType {
        return .fromC(c.wasm_array_type_field(info.raw(), type_index));
    }

    pub fn tagCount(info: *const ModuleInfo) u32 {
        return c.wasm_tag_count(info.raw());
    }

    pub fn tagType(info: *const ModuleInfo, tag_index: u32) ?FuncType {
        return .fromC(c.wasm_tag_functype(info.raw(), tag_index));
    }

    pub fn globalCount(info: *const ModuleInfo) u32 {
        return c.wasm_global_count(info.raw());
    }

    pub fn globalType(info: *const ModuleInfo, global_index: u32) ValType {
        return @fromBackingInt(c.wasm_global_type(info.raw(), global_index));
    }

    pub fn globalIsMutable(info: *const ModuleInfo, global_index: u32) bool {
        return c.wasm_global_is_mutable(info.raw(), global_index) != 0;
    }

    /// Details of a reference global's type, or null when it holds no reference.
    pub fn globalRefType(info: *const ModuleInfo, global_index: u32) ?RefType {
        return .fromC(info.globalType(global_index), c.wasm_global_reftype(info.raw(), global_index));
    }

    pub fn memoryCount(info: *const ModuleInfo) u32 {
        return c.wasm_memory_count(info.raw());
    }

    pub fn tableCount(info: *const ModuleInfo) u32 {
        return c.wasm_table_count(info.raw());
    }

    pub fn tableType(info: *const ModuleInfo, table_index: u32) ValType {
        return @fromBackingInt(c.wasm_table_type(info.raw(), table_index));
    }

    pub fn tableRefType(info: *const ModuleInfo, table_index: u32) ?RefType {
        return .fromC(info.tableType(table_index), c.wasm_table_reftype(info.raw(), table_index));
    }

    /// Looks up and type-checks the exports `Spec` names; see `Plan`.
    pub fn resolve(info: *const ModuleInfo, comptime Spec: type) Error!Plan(Spec) {
        const fields = @typeInfo(Spec).@"struct";
        var plan: Plan(Spec) = undefined;
        inline for (fields.field_names, fields.field_types, 0..) |field_name, T, i| {
            const B = BoundField(T);
            const optional = @typeInfo(B) == .optional;
            const X = if (optional) @typeInfo(B).optional.child else B;
            plan.indices[i] = try info.resolveExport(field_name, X);
            if (!optional and plan.indices[i] == null)
                return info.runtime().fail(error.UndefinedExport, "missing export '" ++ field_name ++ "'");
        }
        return plan;
    }

    fn resolveExport(info: *const ModuleInfo, comptime export_name: [:0]const u8, comptime X: type) Error!?u32 {
        const exp = info.findExport(export_name) orelse return null;
        const rt = info.runtime();
        if (exp.kind != X.extern_kind)
            return rt.fail(error.TypeMismatch, "export '" ++ export_name ++ "' is not a " ++ @tagName(X.extern_kind));
        switch (X.extern_kind) {
            .func => {
                const ft = info.funcType(exp.index) orelse return error.Malformed;
                if (!ft.matches(X.FnType))
                    return rt.fail(error.TypeMismatch, "export '" ++ export_name ++ "' does not match " ++ @typeName(X.FnType));
            },
            .global => if (ValType.of(X.ValueType)) |expected| {
                if (info.globalType(exp.index) != expected)
                    return rt.fail(error.TypeMismatch, "export '" ++ export_name ++ "' is not a global of type " ++ @typeName(X.ValueType));
            },
            else => {},
        }
        return exp.index;
    }
};

/// The bound form of a `Spec` field type: function types become `TypedFunc`, and `TypedFunc`,
/// `Global`, `Memory`, `Table`, and optionals of them stay as they are.
fn BoundField(comptime T: type) type {
    return switch (@typeInfo(T)) {
        .@"fn" => TypedFunc(T),
        .optional => |o| ?BoundField(o.child),
        .@"struct" => if (@hasDecl(T, "extern_kind")) T else @compileError("not an export type: " ++ @typeName(T)),
        else => @compileError("not an export type: " ++ @typeName(T)),
    };
}

/// `Spec`'s exports bound to one instance. Each `Spec` field names an export and gives a function type,
/// `TypedFunc`, `Global(T)`, `Memory`, or `Table`; an optional field is null when the export is missing.
pub fn Bound(comptime Spec: type) type {
    const fields = @typeInfo(Spec).@"struct";
    var types: [fields.field_names.len]type = undefined;
    for (fields.field_types, 0..) |T, i| types[i] = BoundField(T);
    return @Struct(.auto, null, fields.field_names, &types, &@splat(.{}));
}

/// The export indices of `Spec` (see `Bound`), resolved once against a module and valid for each of
/// its instances.
pub fn Plan(comptime Spec: type) type {
    const field_names = @typeInfo(Spec).@"struct".field_names;
    return struct {
        indices: [field_names.len]?u32,

        /// `instance` must be an instance of the module the plan was resolved against.
        pub fn on(plan: @This(), instance: *Instance) Bound(Spec) {
            var bound: Bound(Spec) = undefined;
            inline for (field_names, 0..) |field_name, i| {
                const B = @FieldType(Bound(Spec), field_name);
                if (@typeInfo(B) == .optional) {
                    @field(bound, field_name) = if (plan.indices[i]) |index| .{ .instance = instance, .index = index } else null;
                } else {
                    @field(bound, field_name) = .{ .instance = instance, .index = plan.indices[i].? };
                }
            }
            return bound;
        }
    };
}

/// A decoded and validated module that is not instantiated. It supports introspection, and
/// `Runtime.instantiate` creates instances of it in any runtime, concurrently from several threads.
pub const CompiledModule = opaque {
    /// Free it after the instantiations that use it finish, and before its runtime.
    pub fn deinit(compiled: *CompiledModule) void {
        c.wasm_module_free(compiled.raw());
    }

    pub fn raw(compiled: *const CompiledModule) *c.wasm_module_t {
        return @ptrCast(@constCast(compiled));
    }

    pub fn info(compiled: *const CompiledModule) *const ModuleInfo {
        return @ptrCast(compiled);
    }

    /// The runtime that compiled it.
    pub fn runtime(compiled: *const CompiledModule) *Runtime {
        return compiled.info().runtime();
    }

    pub fn resolve(compiled: *const CompiledModule, comptime Spec: type) Error!Plan(Spec) {
        return compiled.info().resolve(Spec);
    }
};

/// An instantiated module, with its own memories, tables, and globals.
pub const Instance = opaque {
    /// Free instances before the runtime that holds them.
    pub fn deinit(instance: *Instance) void {
        c.wasm_module_free(instance.raw());
    }

    pub fn raw(instance: *Instance) *c.wasm_module_t {
        return @ptrCast(instance);
    }

    pub fn info(instance: *Instance) *const ModuleInfo {
        return @ptrCast(instance);
    }

    pub fn runtime(instance: *Instance) *Runtime {
        return instance.info().runtime();
    }

    fn fail(instance: *Instance, err: Error, message: [:0]const u8) Error {
        return instance.runtime().fail(err, message);
    }

    /// Calls an exported function with untyped values; `results.len` must match its result count.
    pub fn call(instance: *Instance, name: [:0]const u8, args: []const Value, results: []Value) Error!void {
        try check(c.wasm_call(instance.raw(), name.ptr, Value.cPtr(args), @intCast(args.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    pub fn callIndex(instance: *Instance, func_index: u32, args: []const Value, results: []Value) Error!void {
        try check(c.wasm_call_index(instance.raw(), func_index, Value.cPtr(args), @intCast(args.len), Value.cPtrMut(results), @intCast(results.len)));
    }

    /// Looks up an exported function and checks that it has `Fn`'s signature, where a `Value`
    /// parameter or result accepts any type.
    pub fn func(instance: *Instance, name: []const u8, comptime Fn: type) Error!TypedFunc(Fn) {
        const exp = instance.info().findExport(name) orelse return instance.fail(error.UndefinedExport, "no export with that name");
        if (exp.kind != .func) return instance.fail(error.TypeMismatch, "export is not a function");
        return instance.funcAt(exp.index, Fn);
    }

    pub fn funcAt(instance: *Instance, func_index: u32, comptime Fn: type) Error!TypedFunc(Fn) {
        const ft = instance.info().funcType(func_index) orelse return instance.fail(error.UndefinedExport, "function index out of range");
        if (!ft.matches(Fn)) return instance.fail(error.TypeMismatch, "function signature does not match " ++ @typeName(Fn));
        return .{ .instance = instance, .index = func_index };
    }

    /// Binds the exports `Spec` describes; see `Bound`.
    pub fn exportsAs(instance: *Instance, comptime Spec: type) Error!Bound(Spec) {
        return (try instance.info().resolve(Spec)).on(instance);
    }

    /// The exported global `name`, checked to have type `T` (any type for `Value`).
    pub fn global(instance: *Instance, name: []const u8, comptime T: type) Error!Global(T) {
        const exp = instance.info().findExport(name) orelse return instance.fail(error.UndefinedExport, "no export with that name");
        if (exp.kind != .global) return instance.fail(error.TypeMismatch, "export is not a global");
        if (ValType.of(T)) |expected| {
            if (instance.info().globalType(exp.index) != expected) return instance.fail(error.TypeMismatch, "global type does not match " ++ @typeName(T));
        }
        return .{ .instance = instance, .index = exp.index };
    }

    pub fn getGlobal(instance: *Instance, comptime T: type, name: [:0]const u8) Error!T {
        var value: Value = undefined;
        try check(c.wasm_global_get(instance.raw(), name.ptr, @ptrCast(&value)));
        return value.to(T);
    }

    pub fn setGlobal(instance: *Instance, name: [:0]const u8, value: anytype) Error!void {
        try check(c.wasm_global_set(instance.raw(), name.ptr, Value.from(value).toC()));
    }

    pub fn memory(instance: *Instance, memory_index: u32) ?Memory {
        if (memory_index >= instance.info().memoryCount()) return null;
        return .{ .instance = instance, .index = memory_index };
    }

    pub fn table(instance: *Instance, table_index: u32) ?Table {
        if (table_index >= instance.info().tableCount()) return null;
        return .{ .instance = instance, .index = table_index };
    }

    /// Allocates a GC struct of type `type_index` from `fields`, a tuple of values or a `[]const Value`.
    /// GC references stay valid only while Wasm can reach them.
    pub fn structNew(instance: *Instance, type_index: u32, fields: anytype) Error!Value {
        const values = valueArray(fields);
        var ref: Value = undefined;
        try check(c.wasm_struct_new(instance.raw(), type_index, Value.cPtr(&values), @intCast(values.len), @ptrCast(&ref)));
        return ref;
    }

    pub fn structGet(instance: *Instance, ref: Value, field_index: u32) Error!Value {
        var value: Value = undefined;
        try check(c.wasm_struct_get_field(instance.raw(), ref.toC(), field_index, @ptrCast(&value)));
        return value;
    }

    pub fn structSet(instance: *Instance, ref: Value, field_index: u32, value: Value) Error!void {
        try check(c.wasm_struct_set_field(instance.raw(), ref.toC(), field_index, value.toC()));
    }

    /// `elems` is a tuple of values or a `[]const Value`.
    pub fn arrayNew(instance: *Instance, type_index: u32, elems: anytype) Error!Value {
        const values = valueArray(elems);
        var ref: Value = undefined;
        try check(c.wasm_array_new(instance.raw(), type_index, Value.cPtr(&values), @intCast(values.len), @ptrCast(&ref)));
        return ref;
    }

    pub fn arrayLen(instance: *Instance, ref: Value) Error!u32 {
        var len: u32 = undefined;
        try check(c.wasm_array_length(instance.raw(), ref.toC(), &len));
        return len;
    }

    pub fn arrayGet(instance: *Instance, ref: Value, elem_index: u32) Error!Value {
        var value: Value = undefined;
        try check(c.wasm_array_get_elem(instance.raw(), ref.toC(), elem_index, @ptrCast(&value)));
        return value;
    }

    pub fn arraySet(instance: *Instance, ref: Value, elem_index: u32, value: Value) Error!void {
        try check(c.wasm_array_set_elem(instance.raw(), ref.toC(), elem_index, value.toC()));
    }

    pub fn setUserdata(instance: *Instance, ptr: ?*anyopaque) void {
        c.wasm_module_set_userdata(instance.raw(), ptr);
    }

    pub fn userdata(instance: *Instance, comptime T: type) ?*T {
        return @ptrCast(@alignCast(c.wasm_module_userdata(instance.raw())));
    }
};

fn ValueArray(comptime T: type) type {
    const info = @typeInfo(T);
    return if (info == .@"struct" and info.@"struct".is_tuple) [info.@"struct".field_types.len]Value else []const Value;
}

fn valueArray(items: anytype) ValueArray(@TypeOf(items)) {
    const Result = ValueArray(@TypeOf(items));
    if (Result == []const Value) return items;
    var values: Result = undefined;
    inline for (0..values.len) |i| values[i] = .from(items[i]);
    return values;
}

pub fn Iterator(comptime T: type, comptime count: fn (*const ModuleInfo) u32, comptime at: fn (*const ModuleInfo, u32) T) type {
    return struct {
        info: *const ModuleInfo,
        index: u32 = 0,

        pub fn next(self: *@This()) ?T {
            if (self.index >= count(self.info)) return null;
            defer self.index += 1;
            return at(self.info, self.index);
        }
    };
}

/// One of an instance's globals, defined or imported, holding a `T` (any type for `Value`).
pub fn Global(comptime T: type) type {
    return struct {
        instance: *Instance,
        index: u32,

        pub const extern_kind = ExternKind.global;
        pub const ValueType = T;

        const Self = @This();

        pub fn get(self: Self) Error!T {
            var value: Value = undefined;
            try check(c.wasm_global_get_index(self.instance.raw(), self.index, @ptrCast(&value)));
            return value.to(T);
        }

        /// Fails with `error.TypeMismatch` for immutable globals.
        pub fn set(self: Self, value: T) Error!void {
            try check(c.wasm_global_set_index(self.instance.raw(), self.index, Value.from(value).toC()));
        }

        pub fn isMutable(self: Self) bool {
            return self.instance.info().globalIsMutable(self.index);
        }
    };
}

/// One of an instance's linear memories, defined or imported.
pub const Memory = struct {
    instance: *Instance,
    index: u32,

    pub const extern_kind = ExternKind.memory;

    /// The memory's bytes. Growing the memory invalidates the slice unless the memory is shared.
    pub fn data(self: Memory) []u8 {
        const ptr = c.wasm_memory_data(self.instance.raw(), self.index);
        return if (ptr == null) &.{} else ptr[0..@intCast(self.size())];
    }

    /// The size in bytes.
    pub fn size(self: Memory) u64 {
        return c.wasm_memory_size(self.instance.raw(), self.index);
    }

    /// Grows the memory by `delta_pages`, returning the old size in pages, or null when it cannot grow.
    pub fn grow(self: Memory, delta_pages: u64) ?u64 {
        const old = c.wasm_memory_grow(self.instance.raw(), self.index, delta_pages);
        return if (old < 0) null else @intCast(old);
    }

    /// The `len` bytes at `offset`, invalidated as `data` is.
    pub fn slice(self: Memory, offset: u64, len: u64) Error![]u8 {
        const bytes = self.data();
        if (offset > bytes.len or len > bytes.len - offset) return error.OutOfBounds;
        return bytes[@intCast(offset)..][0..@intCast(len)];
    }

    /// Reads a little-endian `T` at `offset`: an integer, float, packed struct, or an array or
    /// extern struct of those.
    pub fn load(self: Memory, comptime T: type, offset: u64) Error!T {
        comptime requirePlainData(T);
        const bytes = try self.slice(offset, @sizeOf(T));
        var value: T = undefined;
        @memcpy(std.mem.asBytes(&value), bytes);
        if (native_endian == .big) std.mem.byteSwap(T, &value);
        return value;
    }

    /// Writes `value` little-endian at `offset`; `T` is as for `load`.
    pub fn store(self: Memory, comptime T: type, offset: u64, value: T) Error!void {
        comptime requirePlainData(T);
        const bytes = try self.slice(offset, @sizeOf(T));
        var copy = value;
        if (native_endian == .big) std.mem.byteSwap(T, &copy);
        @memcpy(bytes, std.mem.asBytes(&copy));
    }

    pub fn read(self: Memory, offset: u64, dst: []u8) Error!void {
        try check(c.wasm_memory_read(self.instance.raw(), self.index, offset, dst.ptr, dst.len));
    }

    pub fn write(self: Memory, offset: u64, src: []const u8) Error!void {
        try check(c.wasm_memory_write(self.instance.raw(), self.index, offset, src.ptr, src.len));
    }

    /// Reads a NUL-terminated string at `offset`, truncated to fit `buffer`.
    pub fn readString(self: Memory, offset: u64, buffer: []u8) Error![:0]const u8 {
        try check(c.wasm_memory_get_string(self.instance.raw(), self.index, offset, buffer.ptr, buffer.len));
        return span(buffer.ptr);
    }

    pub fn raw(self: Memory) *c.wasm_memory_t {
        return c.wasm_module_memory(self.instance.raw(), self.index).?;
    }
};

fn isPlainData(comptime T: type) bool {
    return switch (@typeInfo(T)) {
        .int, .float => @bitSizeOf(T) == 8 * @sizeOf(T),
        .array => |a| isPlainData(a.child),
        .@"struct" => |s| if (s.backing_integer != null) @bitSizeOf(T) == 8 * @sizeOf(T) else s.layout == .@"extern" and for (s.field_types) |F| {
            if (!isPlainData(F)) break false;
        } else true,
        else => false,
    };
}

fn requirePlainData(comptime T: type) void {
    if (!isPlainData(T)) @compileError(@typeName(T) ++ " is not an integer, float, packed struct, or array or extern struct of them");
}

fn guestMemory(instance: *Instance) Error!Memory {
    return instance.memory(0) orelse instance.fail(error.OutOfBounds, "the calling instance has no memory");
}

/// A host function parameter for `len` values of `T` at `offset` in the caller's memory 0, which
/// the guest passes as two `i32` parameters. The range is checked before the function runs.
pub fn GuestSlice(comptime T: type) type {
    comptime requirePlainData(T);
    return struct {
        memory: Memory,
        offset: u32,
        len: u32,

        pub const guest_params = [_]type{ u32, u32 };

        const Self = @This();

        pub fn fromGuest(instance: *Instance, args: []const Value) Error!Self {
            const memory = try guestMemory(instance);
            const self: Self = .{ .memory = memory, .offset = try args[0].to(u32), .len = try args[1].to(u32) };
            _ = try memory.slice(self.offset, @as(u64, self.len) * @sizeOf(T));
            return self;
        }

        /// The slice's bytes, invalidated as `Memory.data` is.
        pub fn bytes(self: Self) []u8 {
            return self.memory.slice(self.offset, @as(u64, self.len) * @sizeOf(T)) catch &.{};
        }

        pub fn get(self: Self, i: u32) Error!T {
            if (i >= self.len) return error.OutOfBounds;
            return self.memory.load(T, @as(u64, self.offset) + @as(u64, i) * @sizeOf(T));
        }

        pub fn set(self: Self, i: u32, value: T) Error!void {
            if (i >= self.len) return error.OutOfBounds;
            return self.memory.store(T, @as(u64, self.offset) + @as(u64, i) * @sizeOf(T), value);
        }
    };
}

pub const GuestBytes = GuestSlice(u8);

/// A host function parameter for a `T` at `offset` in the caller's memory 0, which the guest
/// passes as one `i32` parameter. The range is checked before the function runs.
pub fn GuestPtr(comptime T: type) type {
    comptime requirePlainData(T);
    return struct {
        memory: Memory,
        offset: u32,

        pub const guest_params = [_]type{u32};

        const Self = @This();

        pub fn fromGuest(instance: *Instance, args: []const Value) Error!Self {
            const memory = try guestMemory(instance);
            const self: Self = .{ .memory = memory, .offset = try args[0].to(u32) };
            _ = try memory.slice(self.offset, @sizeOf(T));
            return self;
        }

        pub fn load(self: Self) Error!T {
            return self.memory.load(T, self.offset);
        }

        pub fn store(self: Self, value: T) Error!void {
            return self.memory.store(T, self.offset, value);
        }
    };
}

/// One of an instance's tables, defined or imported.
pub const Table = struct {
    instance: *Instance,
    index: u32,

    pub const extern_kind = ExternKind.table;

    pub fn elemType(self: Table) ValType {
        return self.instance.info().tableType(self.index);
    }

    pub fn size(self: Table) u64 {
        return c.wasm_table_size(self.instance.raw(), self.index);
    }

    /// Reads element `elem_index` as `T`, such as `FuncRef`, `ExternRef`, or `Value`.
    pub fn get(self: Table, comptime T: type, elem_index: u64) Error!T {
        var value: Value = undefined;
        try check(c.wasm_table_get(self.instance.raw(), self.index, elem_index, @ptrCast(&value)));
        return value.to(T);
    }

    /// Stores `value`, which must match the element type.
    pub fn set(self: Table, elem_index: u64, value: anytype) Error!void {
        try check(c.wasm_table_set(self.instance.raw(), self.index, elem_index, Value.from(value).toC()));
    }

    /// Adds `delta` elements set to `init`, returning the old size, or null when the table cannot grow.
    pub fn grow(self: Table, delta: u64, init: anytype) ?u64 {
        const old = c.wasm_table_grow(self.instance.raw(), self.index, delta, Value.from(init).toC());
        return if (old < 0) null else @intCast(old);
    }

    pub fn raw(self: Table) *c.wasm_table_t {
        return c.wasm_module_table(self.instance.raw(), self.index).?;
    }
};

/// Instances of one compiled module, created ahead of use and bound to `Spec`'s exports.
/// Released instances are freed, not reused.
pub fn Pool(comptime Spec: type) type {
    return struct {
        allocator: std.mem.Allocator,
        runtime: *Runtime,
        compiled: *const CompiledModule,
        plan: Plan(Spec),
        idle: std.ArrayList(*Instance) = .empty,

        const Self = @This();

        pub const Lease = struct {
            instance: *Instance,
            exports: Bound(Spec),
        };

        /// Instances are created in `rt`. `compiled` must outlive the pool.
        pub fn init(allocator: std.mem.Allocator, rt: *Runtime, compiled: *const CompiledModule) Error!Self {
            return .{ .allocator = allocator, .runtime = rt, .compiled = compiled, .plan = try compiled.resolve(Spec) };
        }

        pub fn deinit(self: *Self) void {
            for (self.idle.items) |instance| instance.deinit();
            self.idle.deinit(self.allocator);
        }

        /// Instantiates until `count` instances are idle.
        pub fn fill(self: *Self, count: usize) Error!void {
            self.idle.ensureTotalCapacity(self.allocator, count) catch return error.OutOfMemory;
            while (self.idle.items.len < count) self.idle.appendAssumeCapacity(try self.runtime.instantiate(self.compiled));
        }

        /// An idle instance, or a new one when none is idle.
        pub fn acquire(self: *Self) Error!Lease {
            const instance = self.idle.pop() orelse try self.runtime.instantiate(self.compiled);
            return .{ .instance = instance, .exports = self.plan.on(instance) };
        }

        pub fn release(self: *Self, lease: Lease) void {
            _ = self;
            lease.instance.deinit();
        }
    };
}

/// A memory the host creates for `Runtime.bindMemory`. It must outlive every module that imports it.
pub const HostMemory = struct {
    memory: *c.wasm_memory_t,
    allocator: std.mem.Allocator,

    pub const Options = SharedMemory.Options;

    /// `allocator` must outlive the memory.
    pub fn init(allocator: std.mem.Allocator, options: Options) Error!*HostMemory {
        const host = allocator.create(HostMemory) catch return error.OutOfMemory;
        host.allocator = allocator;
        const bridged = AllocatorBridge.wrap(&host.allocator);
        host.memory = c.wasm_memory_new(&bridged, options.pages, options.max_pages, @intFromBool(options.memory64)) orelse {
            allocator.destroy(host);
            return error.OutOfMemory;
        };
        return host;
    }

    pub fn deinit(host: *HostMemory) void {
        c.wasm_memory_free(host.memory);
        host.allocator.destroy(host);
    }

    pub fn raw(host: *HostMemory) *c.wasm_memory_t {
        return host.memory;
    }

    /// The memory's bytes, invalidated when it grows.
    pub fn data(host: *HostMemory) []u8 {
        const base = c.wasm_memory_base(host.memory) orelse return &.{};
        return base[0..@intCast(c.wasm_memory_length(host.memory))];
    }
};

/// A table of null references the host creates for `Runtime.bindTable`. It must outlive every
/// module that imports it.
pub const HostTable = struct {
    table: *c.wasm_table_t,
    allocator: std.mem.Allocator,

    pub const Options = struct {
        elem_type: ValType = .funcref,
        size: u32,
        max_size: ?u32 = null,
    };

    /// `allocator` must outlive the table.
    pub fn init(allocator: std.mem.Allocator, options: Options) Error!*HostTable {
        const host = allocator.create(HostTable) catch return error.OutOfMemory;
        host.allocator = allocator;
        const bridged = AllocatorBridge.wrap(&host.allocator);
        host.table = c.wasm_table_new(&bridged, @backingInt(options.elem_type), options.size, options.max_size orelse std.math.maxInt(u32)) orelse {
            allocator.destroy(host);
            return error.OutOfMemory;
        };
        return host;
    }

    pub fn deinit(host: *HostTable) void {
        c.wasm_table_free(host.table);
        host.allocator.destroy(host);
    }

    pub fn raw(host: *HostTable) *c.wasm_table_t {
        return host.table;
    }
};

/// A memory for the threads proposal, shared by runtimes on separate threads through `Runtime.bindMemory`.
/// It never moves when it grows, and must outlive every module that imports it.
pub const SharedMemory = struct {
    memory: *c.wasm_memory_t,
    allocator: std.mem.Allocator,

    pub const Options = struct {
        pages: u64,
        max_pages: u64,
        memory64: bool = false,
    };

    /// `allocator` must be thread-safe and outlive the memory.
    pub fn init(allocator: std.mem.Allocator, options: Options) Error!*SharedMemory {
        const shared = allocator.create(SharedMemory) catch return error.OutOfMemory;
        shared.allocator = allocator;
        const bridged = AllocatorBridge.wrap(&shared.allocator);
        shared.memory = c.wasm_shared_memory_new(&bridged, options.pages, options.max_pages, @intFromBool(options.memory64)) orelse {
            allocator.destroy(shared);
            return error.OutOfMemory;
        };
        return shared;
    }

    pub fn deinit(shared: *SharedMemory) void {
        c.wasm_memory_free(shared.memory);
        shared.allocator.destroy(shared);
    }

    pub fn raw(shared: *SharedMemory) *c.wasm_memory_t {
        return shared.memory;
    }

    /// The bytes currently in bounds. Another thread may grow the memory, which keeps this slice valid.
    pub fn data(memory: *SharedMemory) []u8 {
        const m = memory.raw();
        const base = c.wasm_memory_base(m) orelse return &.{};
        return base[0..@intCast(c.wasm_memory_length(m))];
    }
};
