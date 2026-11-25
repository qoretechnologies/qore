%modern
%allow-injection
%no-network
%requires ./OwnershipMarker.qm

class Marker {
}

class LocalTag {
}

Program dst(PO_NEW_STYLE | PO_NO_USER_API | PO_ALLOW_INJECTION);
dst.parse("namespace OwnershipMarker { class ForeignTag {} }", "occupied-name");
try {
    dst.importClass("Marker", "OwnershipMarker::ForeignTag", True);
    return 2;
} catch (hash<ExceptionInfo> ex) {
    if (ex.err != "CLASS-IMPORT-ERROR" || ex.desc.find("declaring Program") < 0) {
        return 1;
    }
}

# A regular import still exposes the module's public class.
Program ordinary(PO_NEW_STYLE | PO_NO_USER_API);
ordinary.importClass("OwnershipMarker::ForeignTag", "ImportedTag");
ordinary.parse("bool sub accepts_import(object tag) { return tag instanceof ImportedTag; }", "ordinary-import");
if (!ordinary.callFunction("accepts_import", new OwnershipMarker::ForeignTag())) {
    return 3;
}

# These two classes were declared by this script, including when compiled into a standalone executable.
Program owned(PO_NEW_STYLE | PO_NO_USER_API | PO_ALLOW_INJECTION);
owned.importClass("Marker", "LocalTag", True);
owned.parse("bool sub accepts(object tag) { return tag instanceof LocalTag; }", "owned-markers");
return owned.callFunction("accepts", new LocalTag()) ? 0 : 4;
