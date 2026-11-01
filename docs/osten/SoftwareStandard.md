# OSTen Software Standard
**Working Draft 0.2 | Normative standard for OSTen-native software and system components**

## 1. Fundamental Principle

**Software belongs to the user, not the developer.**

Every application designed specifically for OSTen—and every OSTen-native
component of the operating system—must be understandable, customizable,
portable where applicable, and manageable through OSTen's native tools.

Applications must behave as participants in the OSTen environment rather
than imposing incompatible conventions.

## 2. Universal Resource Editing

Every compliant OSTen application and native operating-system component
must expose its **non-executable, user-facing resources** through the
standard OSTen resource system.

The OSTen Resource Editor shall allow the Owner to inspect and modify:

- Icons, cursors, and graphical assets
- Menus and menu commands
- Dialog boxes and their layouts
- Window layouts and interface controls
- Application strings and labels
- Keyboard shortcut assignments
- Colors, fonts, and appearance resources
- Sound assets and notifications
- Toolbars and other configurable interface components
- Application resource metadata

Resources must use **documented, discoverable, extensible formats**.
A resource's identity must remain stable when its label, ordering,
appearance, or position is changed.

**No native OSTen application or operating-system component should require
source-code modification merely to change its appearance or interface layout.**

Developers must not unnecessarily hardcode user-facing interface elements
that belong in the resource system. Executable application behavior is
distinct from editable resources; changing a resource does not magically
implement a new application command.

## 3. Application Structure

Native applications must be self-contained and recognizable as complete
applications.

- Applications should reside in a single, visible application folder.
- Application-specific preferences belong with the application.
- Installation should normally require extracting an archive and moving
  the application into place.
- Removal should not require a proprietary uninstaller.
- System-wide components must be explicitly identifiable and must expose
  their user-facing resources under this standard.

## 4. Native Interface Consistency

OSTen-native applications must support the established desktop conventions,
including:

- The global menu bar
- Standard Finder interactions
- Consistent keyboard shortcuts
- Standard window behavior
- Native dialogs and interface controls
- System-wide appearance settings
- Drag-and-drop file operations

Developers may introduce additional functionality without discarding
established OSTen conventions.

## 5. User Control and Transparency

Applications must not secretly install background components, alter system
files, or change startup behavior.

Background functionality must be discoverable and controllable by the
system Owner.

System modifications must respect OSTen's System Folder Access mechanism
and applicable filesystem permissions.

**The Owner may intentionally edit, replace, or remove any resource they
are permitted to access—including resources belonging to OSTen and to the
Resource Editor itself.** The Resource Editor must not deny such edits
merely because the chosen result could make an application unusable.

Creating backup copies is the Owner's responsibility. Automatic backups,
undo, recovery mechanisms, and forced safety modes are **not requirements**
of the OSTen Software Standard. If provided, such conveniences must not
remove the Owner's authority over accessible resources.

Resource-editing tools must still accurately read and write the selected
resources without unintentionally altering unrelated data or executable
code.

## 6. Compatibility

OSTen-native compliance applies to applications intentionally built for
the OSTen environment and to OSTen's own native system components.

Compatibility with existing Haiku applications is a separate objective.
Running an existing Haiku application does not automatically make that
application compliant with the OSTen Software Standard.

## 7. Extensibility

The standard must support future resource types, application extensions,
Control Strip modules, Control Panels, and other native components without
requiring a redesign of the application model.

Resource formats, type identifiers, format versions, and associated
application interfaces must be publicly documented.

---

## Guiding Principle

*An OSTen application should never be a mysterious black box. The Owner
should be able to inspect it, customize its experience, understand its
components, and remove it without surrendering control of the system.*

This standard defines the required design of compliant native software.
It does **not** assert that the present development preview is fully
compliant. For current implementation details and outstanding work, see
[SoftwareStandard-ImplementationStatus.md](SoftwareStandard-ImplementationStatus.md).
