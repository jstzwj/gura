# Reference Capabilities for Flexible Memory Management: Extended Version∗

ELLEN ARVIDSSON, Uppsala University, Sweden 

ELIAS CASTEGREN, Uppsala University, Sweden 

SYLVAN CLEBSCH, Microsoft, UK 

SOPHIA DROSSOPOULOU, Imperial College London, England 

JAMES NOBLE, Creative Research & Programming, New Zeeland 

MATTHEW J. PARKINSON, Microsoft, UK 

TOBIAS WRIGSTAD, Uppsala University, Sweden 

Verona is a concurrent object-oriented programming language that organises all the objects in a program into a forest of isolated regions. Memory is managed locally for each region, so programmers can control a program’s memory use by adjusting objects’ partition into regions, and by setting each region’s memory management strategy. A thread can only mutate (allocate, deallocate) objects within one active region — its “window of mutability”. Memory management costs are localised to the active region, ensuring overheads can be predicted and controlled. Moving the mutability window between regions is explicit, so code can be executed wherever it is required, yet programs remain in control of memory use. An ownership type system based on reference capabilities enforces region isolation, controlling aliasing within and between regions, yet supporting objects moving between regions and threads. Data accesses never need expensive atomic operations, and are always thread-safe. 

Additional Key Words and Phrases: memory management, type systems, isolation, ownership 

# 1 INTRODUCTION

Memory management has always been challenging, and programmers and programming language designers have developed a wide range of techniques and patterns to deal with it [Noble and Weir 2000]. Most early languages like FORTRAN and COBOL supported only fixed memory allocation, where memory was allocated before a program began to execute. Algol-60 popularised stack allocation, enabling recursive procedures to be expressed clearly, and then languages like C and Pascal popularised heap allocation, where programmers could manually request memory from the runtime system, and manually return that memory when it was no longer required. LISP introduced the first automatic memory management system—a garbage collector—which relieved programmers from the need to explicitly free memory, rather memory will be automatically reclaimed once it is no longer needed [Jones et al. 2016]. As well as reducing the amount of bookkeeping code programmers have to write, garbage collection typically provides “memory safety” which prevents a number of characteristic errors common to manual memory management, such as failing to free objects that are no longer needed, or accidentally freeing objects that are still in use. 

While there is now a 60+ year history of research in garbage collection algorithms and implementations, many programmers seem resistant to using garbage collection, despite the pitfalls of manual memory management. According to the TIOBE index of programming languages [Tiobe 

2022], about half out of the top twenty programming languages eschew garbage collection, and rely on various forms of manual memory management. The 1st and 3rd of the top 25 Common Weaknesses in CWE 2020–2022 are writing and reading outside of allocated memory and using memory after freeing it is 7th. Memory leaks come in at 32nd and race conditions 33rd [CWE 2022]. 

In short, manual memory management (e.g., C/C++) is unsafe and prone to errors but allows programmers to leverage domain knowledge to optimise memory management. Some compile-time memory management (e.g., Rust) and automatic GC (e.g., Java/C#) avoids the memory unsafety, but instead leads programmers to write unsafe code for a variety of reasons. In Rust, programmers must use unsafe code to construct well-known data structures, and object topologies without clear domination. In Java, programmers use unsafe code [Mastrangelo et al. 2015] to leverage domain knowledge to optimise memory management and to make GC performance more predictable. In general, reasoning about the performance of automatic GC is made difficult by the systemic effects of GC on program performance and the heuristics which control when and how GC is run. 

Contributions. This paper introduces Verona’s region system—Reggio—and its accompanying type system. Reggio gives programmers control over memory management costs by dividing a program’s heap into a flexible forest of independent regions. Programmers can pick a suitable strategy for how memory in each region is managed, irrespective of what other regions do. Within each thread, the programmer explicitly moves a single “window of mutability” through the region forest. The single window of mutability makes clear which region each part of a program is working within, and how the program affects that region, in particular with respect to object liveness, and also permits a flexible aliasing. As a region can only be made accessible through a single pointer, programs become free of data-races by design, and cheap ownership transfer to support reconfiguring the region topology is easy. Memory management overheads (e.g., tracing, and reference count manipulations) are likewise localised to just the mutable region. 

# 2 BACKGROUND

The continuing appeal of manual memory management highlights the research problem we aim to solve: how can languages give programmers the level of control offered by manual memory management, while maintaining memory safety? Two broad research streams tackle this problem, one based on regions and one based on ownership. 

Regions. Gay and Aiken [1998] introduced explicit regions for managing memory in C-like languages: objects are allocated in regions; and entire regions of objects are deleted in one operation, rather than deleting objects individually. A later version of this scheme added annotations to indicate that a pointer refers to an object in the same region, in an enclosing region, or is not allocated via the region system [Gay and Aiken 2001]. Utting [1995] had previously shown how regions could help local reasoning, based on the “collections” or “local stores” used to differentiate pointers in Euclid [Lampson et al. 1977]. 

Rather than using explicit, programmer-visible regions, Tofte and Talpin [2004; 1997] demonstrated how Milner-style inference [Baker 1990] could be extended to implicitly allocate objects to regions, and allocate and deallocate those regions, without either explicit first-class regions, or additional annotations on programs or types. Their MLKit [Tofte et al. 2021] runs ML programs using stack allocation, as the regions are allocated last-in, first-out. Because these inferred regions are implicit, the region structure does not capture a programmer’s intent about how and where memory should be allocated and freed. MLKit remains under continuous development, in particular showing how regions can be supported in a straightforward monadic style [Fluet and Morrisett 2006], and integrating generational-style GC within regions [Elsman and Hallenberg 2021]. 

Safe region allocation was then tested at scale in Cyclone [Grossman et al. 2002] an extension of C with an explicit region construct, rather than using inference. Like the MLKit, Cyclone regions were originally stack based; later versions also adopted support for unique pointers and reference counted objects to permit deallocation of individual objects inside a region, at the cost or introducing memory leaks due to cycles or failure to deallocate a dropped unique pointer [Hicks et al. 2004]. Both MLKit style implicit / inferred regions, and Cyclone explicit / annotated regions can be modelled by a common core calculus based on linear references to explicit, first-class regions [Fluet et al. 2006]. 

Ownership. Work on object ownership effectively begins with Hogg’s Islands [1991] and a general recognition of the need to control topologies of programs [Hogg et al. 1992] in languages where object identity (i.e., dynamic allocation, mutable state), encapsulation, and even “automatic storage management” are taken as essential design principles, rather than accidental optimisations [Ingalls 1981; Lehrmann Madsen et al. 1993; Lieberherr and Holland 1989]. 

Based on “Flexible Alias Protection” [Noble et al. 1998], ownership types [Clarke 2001; Clarke et al. 1998] offer compile-time enforcement of pointer encapsulation, including the property that, considering paths through the object graph, an “owner” object should dominate all other objects that programmers intend to encapsulate “inside” it [Potter et al. 1998]. Leveraging “owners-as-dominators”, extensions to ownership types have been applied to encapsulate object invariants [Müller and Poetzsch-Heffter 1999], record conformance to software architecture [Aldrich and Chambers 2004], localise program effects [Clarke and Drossopoulou 2002], scope object cloning [Li et al. 2012], ensure actor isolation [Clarke et al. 2008; Gruber and Boyer 2013; Srinivasan and Mycroft 2008], prevent data races [Boyapati and Rinard 2001; Gonnord et al. 2023], support safe parallelisation [Bocchino 2011; Francis-Landau et al. 2016] or ensure data is only accessed under a mediating lock [Flanagan and Freund 2000]—the first fifteen years of these efforts are surveyed in [Clarke et al. 2013]. Ownersas-dominators leads directly to applications in memory management, as deleting a dominating node from a graph, by definition, must also delete every node it dominates. This was first demonstrated in SafeJava [Boyapati et al. 2003] using ownership types to compile straightforwardly annotated programs to the Real-Time Specification for Java [Bollella et al. 2003], which supports fine-grained control over memory via explicit dynamically-scoped regions. 

Distinguishing between the inside and outside of an encapsulated object lets languages generalise traditional pointer-based uniqueness to external uniqueness [Clarke and Wrigstad 2003], where an object may have only one pointer from the outside, but any number of pointers from its inside. As with regions for unique objects, an externally unique object can be represented as the sole object in a region; however for external uniqueness, the object’s region can also contain one or more enclosed subregions in turn containing the object’s insides. External uniqueness is almost as strong as regular uniqueness [Wrigstad 2006], and in particular makes it easier to change objects’ ownership, or dually, to transfer objects between actors or independent threads [Clarke et al. 2008; Gordon et al. 2012; Haller and Loiko 2016; Haller and Odersky 2010]. 

Rust. Regions and ownership have been brought together recently in the design of Rust, combining control of memory use, safe concurrency, and excellent compiler error messages [Hu 2020; Klabnik and Nichols 2019; Krill 2021; Turon 2015]. Rust essentially inherits Cyclone’s and MLKit’s regions, but strongly integrated with uniqueness. In particular, only values which are uniquely referenced or passed by copy are mutable. Nested uniqueness brings nested ownership: a unique value is owned by its storage location, ensuring that when an owner is deallocated, all the memory owned by that owner can also be deallocated. 

To make programming in Rust practical, Rust allows unique values to be borrowed without nullifying their source: a unique mutable reference can be passed up the stack without losing uniqueness [Boyland 2001] or traded for multiple read-only borrowed references [Wadler 1990]. 

To ensure absence of dangling pointers, Rust tracks lifetimes of borrowed references and ensure that a “longer-lived” (or enclosing) object can never point to a “shorter-lived” (or encapsulated) object. This borrowing semantics is reminiscent of fractional permissions [Boyland 2013]. Through uniqueness, Rust imposes a multiple-reader/single-writer concurrency model [Lea 1998]. 

The strict rules surrounding ownership and borrowing, and compilers’ inability to accept safe programs that they “cannot understand”, make Rust hard to learn and to use correctly [Abtahi and Dietz 2020; Blaser 2019; Coblenz et al. 2022; Jung et al. 2020; Qin et al. 2020; Spencer 2020]—to the point where the difficulty of implementing first-year data structures (such as doubly-linked lists) has now become an Internet trope [Beingessner 2019; Cameron 2015; Cohen 2018; ndrewxie 2019]. When faced with these problems in practice, Rust programmers either escape into unsafe Rust or revert to the birthplace of aliasing, using integer array indices, FORTRAN style [Bendersky 2021]. 

# 3 REGGIO REGIONS

In this section we describe the central concepts of the Reggio region system: regions, the region topology, operations on regions, the single window of mutability, and the properties of the region system. But first, let us overview the goals of this work. 

# 3.1 Motivation

The design decisions in this paper are motivated by our primary goal: 

(G1) Controllable and Predictable Memory Management Costs. It should be possible for a programmer to reason about and control the impact of memory management on performance. 

Our approach is to divide a program’s heap into isolated regions and make each region an isolated unit of memory management. Concretely, we set the following five sub-goals: 

(G2) Mix-and-Match Memory Management. A region is free to manage its own memory however it likes, irrespective of any other regions in the program. Thus, a programmer is free to pick a memory management strategy suited to the needs of particular operations. 

(G3) Incremental Memory Management. Performance of memory management in one region should not be affected by activities in another region. Thus, fine-grained partitioning gives finer cost-control. 

(G4) Zero-Copy Ownership Transfer. Ownership transfer between regions must be possible without copying objects. Thus, fine-grained partitioning does not have a hidden expensive cost, and heap topology can be modified cheaply. 

(G5) Concurrent Memory Management. Timing of memory management in one region should not be contingent by activities in another region. Thus, a programmer can initiate an operation— memory management or not—without having to wait, or forcing a wait upon any other part of the program. 

(G6) Safe Concurrency. A thread that has access to a datum may access it freely without any need for synchronisation, and with a guarantee of data-race freedom. 

Because this paper does not deal explicitly with concurrency, we will refrain from discussing the last two goals until § 8.1. 

# 3.2 High-Level Overview

We distinguish between mutable and immutable objects. In this paper, we are mostly concerned with the former. Immutable objects do not live in regions, and can be accessed freely in a program. In contrast, every mutable object belongs to a particular region. In certain circumstances, mutable objects may be made temporarily immutable. To avoid confusion, we will sometimes use the phrase permanently immutable to denote objects whose mutability is irretrievably lost. 

3.2.1 Regions and Region Topology. A region is a set of objects whose memory is managed together. At any moment, one of these objects is designated as the bridge object. A region can be opened or closed. Closed regions are isolated from the rest of the program which means that with the exception of the bridge object, objects in a closed region are only referenced from within the region. Bridge objects are externally unique [Clarke and Wrigstad 2003] so they may have an additional, single external incoming reference. 

The only outgoing references from objects in a region are either to immutable objects or to bridge objects of other (nested) regions. Thus, a program’s region topology forms a forest, and moving the external reference to the bridge object changes the topology. The topology of references within regions is unrestricted: any object can point to any other within the same region. 

Fig. 1 illustrates the isolation of the region ?? (bean-shaped boundary). Object ?? is the current bridge object of the region, denoted by drawing it on the boundary. Also ?? and ?? could serve as the bridge object. References $e \longrightarrow n$ and $m \to e$ are not permitted as they break isolation. The reference from $a \to i$ is permitted as ?? is immutable. The reference ?? → ?? is permitted because ?? is the current bridge object. Immutable objects do not live in regions and may only refer to other immutable objects. Therefore, the reference $o \to$ ?? is not allowed. Last, bridge objects may only have one incoming reference from outside the region, so no more references to ?? are allowed from outside of ??, regardless of their origin. 

3.2.2 Regions and Memory Management. Every region manages the memory of its objects in isolation, and according to a strategy picked by the programmer specifically for that region at the time of its creation. 

Code inside of a region is agnostic to how memory is managed, meaning that a library can leave such decisions to its users. 

When an external reference to a bridge object is dropped, the entire corresponding region can be free’d along with any nested regions. In Fig. 1, dropping ?? → ?? makes all objects in region ?? unreachable as external references to its objects $( e . g . , m  e )$ are not permitted. Thus, they can be collected immediately. 

3.2.3 Single Window of Mutability. A region must be explicitly opened to be accessed, and must be closed before it can be opened again. The open regions form a LIFO stack. The top region is active and the remaining regions on the stack are suspended. An active region permits allocation, deallocation and mutation of its objects. When a region is suspended, neither allocation, deallocation nor mutation is permitted in it. 

Making a region active temporarily weakens its outgoing encapsulation: its objects are permitted to reference objects in the suspended regions further down the stack. Suspending a region conversely weakens its incoming encapsulation: its objects can be referenced by the regions further up the stack. Table 1 overviews the allowed actions depending on a whether a region is active, suspended or closed. When the active region is closed, it is popped from the stack, and the new top region goes from suspended to active. Because closed regions are not permitted outgoing refereces, any references to objects in open regions must be invalidated. 

To the active region, the suspended regions appear as a single temporarily immutable region whose objects can be referenced as long as the active region remains on the stack. Programmers can thus trade mutability for access, and any object can be temporarily accessed from anywhere, provided the containing regions are opened on the stack in a permitting order. 

In Fig. 1, to open ?? we must first open $R ^ { \prime }$ to access the reference $m  a .$ Opening ?? through this reference suspends $R ^ { \prime }$ , making ?? and ?? temporarily immutable, $a , c ,$ and ?? mutable, and permitting $e \longrightarrow n$ . With the topology of Fig. 1, we cannot open ?? and $R ^ { \prime }$ in a way that permits the creation of $m \to e$ as ?? is immutable when $R ^ { \prime }$ is suspended, and ?? is not accessible when ?? is closed. To do so, we must change the topology by moving $m \to a$ out of $R ^ { \prime } , e . g .$ , to a stack variable or other region. 

![](images/435342c17a0c4bfa1fb783ee32b4a5a66a9e80982a7643c685e492f003e63d8e.jpg)



Fig. 1. Region isolation of two closed regions.



Table 1. Allowed actions depending on a region’s state. Incoming and outgoing denote references to mutable objects from and to other regions respectively. Bridge means only to bridge objects; $\mathsf { a n y } ^ { * }$ means to any object of a previously (outgoing) or subsequently (incoming) opened region. Free object denotes ability to free individual objects inside of a region. Free region denotes the ability to free an entire region.


<table><tr><td rowspan="2">State</td><td colspan="2">Encapsulation</td><td colspan="2">Effects</td><td colspan="3">Memory Management</td><td rowspan="2">Nested Regions</td></tr><tr><td>Incoming</td><td>Outgoing</td><td>Mutate</td><td>Read</td><td>Alloc object</td><td>Free object</td><td>Free region</td></tr><tr><td>active</td><td>bridge</td><td>any*</td><td>yes</td><td>yes</td><td>yes</td><td>yes</td><td>no</td><td>yes</td></tr><tr><td>suspended</td><td>any*</td><td>any*</td><td>no</td><td>yes</td><td>no</td><td>no</td><td>no</td><td>yes</td></tr><tr><td>closed</td><td>bridge</td><td>bridge</td><td>no</td><td>no</td><td>no</td><td>no</td><td>yes</td><td>yes</td></tr></table>

In combination, the design decision to only permit mutation in one region at a time, the LIFO order of the region stack and the inaccessibility of closed regions facilitates reasoning about sideeffects. The main motivation, however, is to control the costs of memory management. As we shall see, direct overheads related to memory management such as maintaining reference counts or tracing object structures are only applicable to active regions. 

3.2.4 Navigating Through the Region Forest. Due to the single window of mutability, programs require explicit navigation through the region forest. The left subfigure of Fig. 2 shows Fig. 1, denoting regions’ states by colour when $R ^ { \prime }$ is active and $R$ is closed. The white box denotes the stack frame of $R ^ { \prime }$ with its local variable(s). If we proceed by opening ?? we arrive at the right subfigure of Fig. 2: a new top frame is created inside ?? containing its own local variables, with y holding a reference to ??; $R ^ { \prime }$ is suspended and ?? active, and the window of mutability is moved from $R ^ { \prime }$ to ??. The reference $z \longrightarrow m$ shows the weakened 

isolation allowing outgoing references from ?? and incoming references to $R ^ { \prime }$ . To continue, we may close ?? or open any reachable region $R ^ { \prime \prime }$ . The former will invalidate any references from ?? to $R ^ { \prime }$ since these would violate isolation. The latter gives mutable access to $R ^ { \prime \prime }$ and suspends $R ,$ and permits references from $R ^ { \prime \prime }$ to both ?? and $R ^ { \prime }$ . 

![](images/088d26be3f84115e8ced8f747713f2eabcfb4c6ae5eef4f609bf6715a344156f.jpg)



Fig. 2. Left: $R ^ { \prime }$ is active and ?? is closed. Right: we opened ?? making it active and suspending $R ^ { \prime }$ .


![](images/5b7e8e962aec989f7fed5e0b7b055864d09e358a82d182c817680fafaea955f1.jpg)



Fig. 3. Examples of bridge swapping. Time moves left; we use different shades of blue for clarity. Subfigures 1–3 construct the cyclic linked list $[ a , b , c ]$ , using the most recently added link as the bridge object. Subfigures 4–6 construct an iterator, for the list, and make it the bridge object. We subsequently use the iterator to iterate to the ?? link and unlink $\mathrm { i t , }$ before dropping the iterator and making ?? the bridge object (and with a garbage iterator whose removal is determined by the region’s memory management strategy).


3.2.5 Swapping the Bridge Object. Any object in a region can serve as the bridge object. While a region is open, any object can be designated as the new bridge object, and we make this choice visible when the region closes. Thus, it is possible to $e . g .$ , create a region with a stack where the bridge object is always the top link. If we wish to create an external iterator to the stack, we can create an iterator inside the region, and make the iterator the bridge object for the duration of the iteration, and then switch back again. Fig. 3 shows the situation pictorially. 

3.2.6 Merging and Freezing Regions. A closed region’s contents may be merged into another region by dropping the uniqueness of the bridge object. For clarity, we use an explicit merge operation. Fig. 4 shows merging ??′ into ??, which moves the objects in ??′ into ?? and creating the (now legal) ?? → ?? reference from the variable x, after which ??′ ceases to exist. Merging a region (source) into another (sink) moves all regions directly nested in the source to the sink, but does not merge those regions into the sink (see $R ^ { \prime \prime }$ in Fig. 4). 

Permanently immutable structures are created by constructing a mutable region and then turning its entire contents immutable through an explicit freeze operation that operates on closed regions. In contrast to merging, freezing a region also freezes its nested regions (see Fig. 4). This preserves the property that immutable objects may only reference other immutable objects. Freezing dissolves region boundaries, making the frozen objects freely accessible to objects in all regions. 

![](images/2900cf2d9c8d9b7f811468b9f11764ed9279bb814d18d14e85a2c7231c1b7183.jpg)


![](images/7add51af8903710e4a9f84011d816cddb7dd093d52a57b61ddcff785d683b758.jpg)



$\mathsf { m e r g e } R ^ { \prime } \mathsf { i n t o } R - R ^ { \prime \prime } i s u n a H e c t e d$


![](images/5aa0a38623f787aa237154e3d046b6f869f0569145ea78438a7983f306482a22.jpg)



freeze R’ also freezes R’’



Fig. 4. Left: Three nested regions ??, ??′, and $R ^ { \prime \prime } .$ . Centre: Merging region $R ^ { \prime }$ into ??. Right: Freezing ??′.


# 4 REFERENCE CAPABILITIES FOR STATICALLY ENFORCING REGION ISOLATION

We now introduce a type system that statically enforces region isolation according to the encapsulation and effects columns of Table 1. A region is opened through an enter operation that takes a reference to bridge object and a lambda, executes the lambda inside the region passing it the bridge object as argument, and then closes the region. Its companion operation explore opens a region in a suspended state (while still suspending the former active region). 

Our type system uses reference capabilities which decorate all types ?? in a program: 

– mut ?? denotes an intra-regional reference to an object of type ?? (r, c, e, and m in Listing 1); 

– tmp ?? is like mut ??, but the lifetime of the object is bound to the enter/explore scope; 

– imm ?? denotes a reference to a permanently immutable object of type $\tau ( \mathrm { i }$ and o); 

– iso ?? denotes an externally unique reference to a bridge object of type ?? of a closed region (a); 

– paused ?? denotes a reference to an immutable object in a suspended region (z in Fig. 2). 

On assignment, the lhs capability and the rhs capability must be the same. merge has the signature iso→mut and freeze has the signature iso→imm. To enter or explore, an iso is needed. 

All expressions are typed from the point of view of the currently active region. A field f declared with a mut capability will only appear as such when the object ?? containing f is in the active region. If ??’s containing region is suspended, f will also appear as suspended; if $\boldsymbol { o ^ { \prime } s }$ containing region is closed, f is not even visible to the program. This is handled by viewpoint adaptation $( c . f . , \ S 4 . 1 )$ . 

An invariant in our system is that aliases to an object that are accessible simultaneously have the same capability. This design is motivated in-part by region isolation and in-part by a desire to keep complexity down in this presentation. For example: If a mut and paused could alias, the immutability of the latter would be weakened to read-only. If paused and imm could alias, it would violate the temporary vs. permanent nature of their immutability. (This restriction could be relaxed to permit some aliasing across the mutable capabilities and aliasing across immutable capabilities.) 

Constructing Fig. 1. Listing 1 shows code that creates the regions, objects, and (permitted) references of Fig. 1. Line 4 creates the ?? region. On Line 2, the immutable object ?? is constructed by creating a region and freezing it. The object ?? is created similarly. Its reference to ?? does not break region isolation as ?? is immutable. We could get rid of the freeze on Line 2 since Line 3 moves ?? into ??’s region and then freezes the entire structure. 

When a region is created, it is closed and empty, except for its bridge object. To populate ?? as in Fig. 1, it must first be made active. The enter keyword is used to open a region and making it active. It takes a unique reference to a bridge object as its argument. Lines 6–11 of Listing 1 show the use 

// Freeze creates immutable objects, c.f. §3.2.6
let i = freeze new iso Cell(42)
let o = freeze new iso Cell(i)
let a = new iso Link // creates R
// {r => ... } is a lambda with argument r
enter a { r => let c = new mut Link
    let e = new mut Link
    r.elem := i
    r.next := c
    c.next := e
    e.next := r }
let m = new mut Cell(a) // buries a 


Listing 1. Code creating the region ?? from ??′ as depicted in Fig. 1.


of an enter block to open the ?? region to allocate and mutate its objects. (The code executes with region ?? active and region ??′ suspended.) 

Entering a region moves control inside it and places a mut reference to its bridge object in a variable on the stack (r) that can be used to call methods, or obtain and store references to other objects in the region. Exiting the enter block (after Line 11) closes the region, and moves control back to the previous region. While a region is open, the external reference to its bridge object is buried [Boyland 2001], meaning it is not accessible to the program. 

Upon exiting the enter block, all variables referencing objects in the now-closed region (c, e, and r) are invalidated, save for the reference to the bridge object in a. Any temporarily permitted references to objects in suspended regions, such as ?? → ?? in Fig. 1, will be invalidated as well. (We will show how this is enforced statically in §4.3.) 

# 4.1 Controlling Effects through Viewpoint Adaptation

We rely on viewpoint adaptation [Dietl et al. 2007] to capture how a reference’s type changes depending on its enclosing region’s relation to the active region. Viewpoint adaptation means that the type of an object may appear differently depending on from where it is accessed. For example, when accessed through a variable of type imm, a field declared with type mut appears as imm. (This particular case ensures that turning a unique reference to a bridge object immutable will propagate the immutability to the entire region and nested regions.) 


Table 2. Viewpoint adaptation. If the capabilities of x and f are ?? and ??, then the capability of x.f is ?? ⊙ ?? = ??, which we read as “?? sees ?? as ??.” For †, c.f., §4.3. The meaning of ⊥ is inaccessible; For ⊥/iso see text.


<table><tr><td rowspan="2">Capability on x</td><td colspan="5">Capability on f</td></tr><tr><td>mut</td><td>tmp</td><td>imm</td><td>iso</td><td>paused</td></tr><tr><td>mut</td><td>mut</td><td><eq>\bot\dagger</eq></td><td>imm</td><td><eq>\bot/iso</eq></td><td><eq>\bot\dagger</eq></td></tr><tr><td>tmp</td><td>mut</td><td>tmp</td><td>imm</td><td><eq>\bot/iso</eq></td><td>paused</td></tr><tr><td>imm</td><td>imm</td><td>imm</td><td>imm</td><td>imm</td><td>imm</td></tr><tr><td>iso</td><td><eq>\bot</eq></td><td><eq>\bot</eq></td><td><eq>\bot</eq></td><td><eq>\bot</eq></td><td><eq>\bot</eq></td></tr><tr><td>paused</td><td>paused</td><td>paused</td><td>imm</td><td><eq>\bot/iso</eq></td><td>paused</td></tr></table>

Viewpoint adaptation also changes the types of variables captured by an enter or explore block to propagate suspension. Captured isos retain their iso-ness rather than become paused. Table 4 shows the viewpoint adaptation rules. The meaning of ⊥/iso is that an iso location is inaccessible through a mut, tmp or paused, unless it is swapped, buried or borrowed (c.f., §4.2). 

To illustrate viewpoint adaptation, consider A enter $\texttt { x \{ y \Rightarrow \mathcal { B } \} }$ . In scope ${ \mathcal { A } } _ { : }$ , let the variables x and v have the types iso $\tau _ { 1 }$ and $k \tau _ { 2 }$ respectively. In scope B, x is undefined and y is introduced with type mut $\tau _ { 1 }$ . This reflects the region pointed to by x going from closed (denoted by x being iso) to active (denoted by y being mut). Moving control from A to B suspends the region active in A (denoted there by mut and tmp). Thus, in scope B, the type of v is (paused $\odot k ) \tau _ { 2 }$ . Through a paused reference, objects in the same region are paused. paused and imm references stay paused and imm respectively (permanently immutable is stronger than temporarily immutable). iso references stay iso. This permits opening nested regions of a suspended region. 

If $k = \mathrm { i } s 0 ,$ , then v.f is not typeable in neither $\mathcal { A }$ nor B as iso $\odot k ^ { \prime } = \perp$ , regardless of what $k ^ { \prime }$ (the capability of f) is. This is as expected, as iso’s cannot be dereferenced (they must be enter’d). 

# 4.2 Region Isolation and Bridge Object External Uniqueness

Regions which are closed or active only have a single incoming reference, which goes to the bridge object. Thus, when a region is closed, it can be moved in and out of other regions by moving its single incoming reference. When a region is opened, its containing region is suspended, which means that the object containing field holding the reference to the bridge object is paused so the field cannot be reassigned. Thus, regions cannot move while open. Finally, when a region is suspended, incoming references are permitted from the stack and heap of subsequently opened regions $( c . f . , \ S 4 . 3 )$ . As regions are opened using a lexically scoped construct (enter or explore), regions are opened and closed in LIFO order. This means that when a suspended region becomes active again, the permitted incoming aliases that could be declared in the block have gone out of scope, and the region’s bridge object is again the only incoming alias. 

As shown in Table 3 uniqueness of bridge object references is maintained by a combination of swapping, burying, and borrowing. 


Table 3. Maintaining uniqueness of bridge object references.


<table><tr><td>Swap[Harms and Weide 1991]</td><td>Reading a mutable variable containing an iso requires that its contents is replaced. For example, y = x is not permitted when x is iso. However, y = x = v is, which replaces the value of x by the value v and moves the previous value of x into y.</td></tr><tr><td>Bury[Boyland 2001]</td><td>Reading a let-bound variable with an iso invalidates the variable. For example, foo(x, x) is not permitted when x is iso as the second use of x cannot be typed.</td></tr><tr><td>Borrow[Wadler 1990]</td><td>Dereferencing an iso requires opening its region, where aliasing of the bridge object is unrestricted, and region isolation protects aliases to the bridge object from escaping.</td></tr></table>

Entering a region borrows and/or buries the variable or field referencing the bridge object. In the case of a stack variable, the variable is buried to prevent the region from being multiply opened. In the case of a field, we instead resort to a dynamic check of the region’s state. If the region is closed, it may be opened. If the region is already open, an exception is thrown. 

# 4.3 Temporary Objects Allow References to Suspended Regions on the Heap

As exemplified already, we permit local variables to store references to objects in a suspended region $( e . g . , z$ in Fig. 2). This is sound as objects in suspended regions are immutable, and because local variables in the active region are guaranteed to go out of scope before a region opened by an enclosing enter or explore becomes active (and thus mutable), as explained above. 

Because objects with tmp capability are created in, and bounded by, a lexical scope, they have the same lifetimes as local variables declared therein. Therefore, we can grant the same permissions to store references to suspended regions to tmp objects. We permit accessing paused and tmp fields through a tmp, but not through a mut (as shown in Table 4). Permitting a mut object to store a suspended reference could lead to a breach of region isolation (see §4.5 for an example). Thus from ??1 ⊙ ??2 = tmp it follows that $k _ { 1 } = \mathrm { t m p }$ . 

In terms of Fig. 1, making ?? a temporary object allows its fields to store references with tmp capability. This permits the reference ?? → ?? when ?? is active and ??′ is suspended. However, ?? → ?? would no longer be permitted unless ?? is also tmp, etc. 

# 4.4 Storage Locations, Strong Updates and Bridge Object Swapping

We unify the treatment of mutable variables (denoted var as opposed to let) and fields through a storage location abstraction (similar to a pointer to a variable or field in C). 

Storage locations are typed ?? Store $\nsim \tau ]$ where ?? is the capability of the frame or object containing the location and ??′ is the capability of the value stored at the location. 

We add a new capability that we call var for use in mutable local variables. var differs from tmp in that it supports strong updates. Its viewpoint adaptation rule is var ⊙ ?? = ?? (“var sees ?? as ??”). 

As shown in Listing 2, a mutable local variable x holding a ??-typed value has the type var Store[??]. Storage locations are subject to the normal rules for viewpoint adaptation, so opening another region when x is already in scope will change the type of x to paused Store[??]. We introduce a dereference operator * and an update operator := to access the contents of a storage location. A storage location must be mut, tmp, or var to be updated. We apply viewpoint adaptation to type the result of dereferencing. On Line 7, *x has type (paused ⊙ mut) Cell, i.e., paused Cell. 

We support changing the bridge object of a region—including changing it for an object of a different type—by presenting the borrowed bridge object reference internally in the enter block as a var storage location. (Note that it is not possible to update the bridge object in an explore as it opens the region as suspended.) 

Line 6 shows that changing the bridge object to an object of another type is possible by simply assigning to y. Strong updates of fields are not 

possible, and this is handled by using tmp Store[. . .] instead of var Store[. . .] to type a bridge object reference borrowed from a field as opposed to a stack variable. 

```txt
var u = new iso Cell(42) // var Store[iso Cell]  
var x = new Cell(4711) // var Store[mut Cell]  
enter u { y =>  
    // y has type var Store[mut Cell]  
    // x has type paused Store[mut Cell]  
    y := new Foo(*y) // changes bridge object('s type)  
    y := *x // rejected: *x is paused Cell, not mut Cell  
    x := ... // rejected: the x storage location is paused  
} // change to u becomes visible 
```


Listing 2. Storage locations example.


# 4.5 Types Enforce Region Isolation and the Single Window of Mutability

Region isolation means no references into active or closed regions from other regions (modulo unique references to bridge objects) or from immutable objects, and no outgoing references from closed regions to open regions. Let’s see how our types enforce this, by looking at ?? in Listing 1. 

The newly created region ?? (Line 4) is isolated as iso constructors only accept iso’s and imm’s as arguments. Right after creation, ?? is closed, and its only external reference is iso. Since iso’s cannot be the receivers of method calls or field accesses, we cannot read or write internal objects in ??, so we cannot create the illegal references ?? → ?? or ?? → ??. 

When ?? is active (Line 6–11), all previously suspended regions stay suspended (none in Listing 1), and are joined by the current region (??′). This is captured by viewpoint adaptation which changes all variables which are mut, tmp, or var in the enclosing region to paused. This prevents these variables from being updated, and reading them yields paused references. Field updates via paused or imm references are not allowed, and method calls on such references require that the method’s self type matches the external view, meaning any callable method cannot perform a field update on self (or call such a method). Thus, we cannot create ?? → ?? or ?? → ??. 

As permitted by our definition of region isolation, we may store paused references in the fields of tmp objects in ?? while ?? is active $( e . g . , e \to n$ if ?? is created as tmp on Line 7). These references will be invalidated when ?? closes as tmp references can only be stored in variables local to the enter block (since mutable variables in the enclosing scope have been suspended), or in other tmp objects $( i . e . , a  c  e$ is an impossible path if ?? is tmp, as the ?? object is mut by definition). 

If we did not invalidate references into suspended regions such as $e \longrightarrow n ,$ , we could circumvent region encapsulation. For example, imagine closing ?? (without invalidating $e \to n )$ , then closing $R ^ { \prime }$ while moving ?? out of $R ^ { \prime }$ . Then reopen ?? without first opening $R ^ { \prime }$ . Now $e \to$ ?? would constitute a reference into the internals of a closed region, thereby breaking region isolation. 

Inside an enter or explore block the enclosing scope is immutable. Together with region isolation this gives that only one region at a time is mutable, i.e., a “single window of mutability”. 

Last, region isolation means that reassigning the pointer to the externally unique bridge object effectively changes the region topology of the heap (G4). 

# 5 THE USE OF REGIONS FOR MANAGING LIVENESS

We have sketched how our type system enforces region isolation and the single window of mutability. In this section, we will show how this translates to costs for managing liveness when considering memory management in isolated regions. 

Fig. 5 shows a heap consisting of regions $R _ { 1 }$ to $R _ { 6 }$ . Presently, the program has entered $R _ { 1 } , R _ { 2 } , R _ { 3 }$ and $R _ { 4 }$ in that order. Ignoring method call indirections, we can imagine a corresponding program shape starting in $R _ { 1 } \colon \mathcal { R } _ { 1 }$ enter $\mathrm { ~  ~ \sf ~ x ~ } \{ \mathrm { ~  ~ d ~ } \Rightarrow \mathrm { ~  ~ \mathcal { R } _ { 2 } ~ }$ enter $\begin{array} { r l } { \textsf { e . f } \{ \textsf { f } \textsf { f } \Rightarrow \textsf { \mathcal { R } } _ { 3 } } & { { } } \end{array}$ enter $\begin{array} { r }  \mathsf { e . g } \ \{ \textbf { g } \Rightarrow \ \mathcal { R } _ { 4 } \ \} \ \textbf  \} \textbf { \ } \} \end{array}$ . 

The region stack is thus $[ R _ { 4 } , R _ { 3 } , R _ { 2 } , R _ { 1 } ]$ where $R _ { 4 }$ is active. Region isolation prevents references from objects in region ?? to objects in region ?? if $i < j$ with the exception of the bridge object references: $\mathsf { x } \to d , e \to f$ , and $e \longrightarrow g$ . The first of these is made inaccessible right after the first enter by making x undefined in $\mathcal { R } _ { 2 }$ and nested scopes. While $R _ { 4 }$ is open, $R _ { 2 }$ and $R _ { 3 }$ are suspended, meaning the fields holding $e \to f$ and $e \longrightarrow g$ cannot be reassigned (static check). Furthermore, they cannot be dereferenced (i.e., opened) since the regions are already open (dynamic check). Thus, while $R _ { 4 }$ is open, the incoming references to the bridge objects will remain the same, i.e., the path that holds the region alive is stable.1 

![](images/a0dc6b50900cc79ed208f4c820ac758ef8aeebc03b93f9826c3aeafa09591727.jpg)



Fig. 5. Program with 4 open and 2 closed regions.


Because of this, as long as $R _ { 4 }$ remains active, liveness of objects in regions $R _ { 1 } { - } R _ { 3 }$ and $R _ { 5 }$ is invariant as no activity in $R _ { 4 }$ can cause objects in these regions to become garbage.2 This does not hold for $R _ { 6 } { \mathrm { : } }$ , as $R _ { 4 }$ could drop $h \to i$ to make the entire region $R _ { 6 }$ garbage. It does hold for $b \to c$ however, since ?? is in a suspended region (?? is temporarily immutable). 

Furthermore, because of the absence of references from $R _ { 1 } { - } R _ { 3 }$ into $R _ { 4 } { \mathrm { - w i t h } }$ the exceptions of the stable bridge object references that are either buried or cannot be re-opened, objects in $R _ { 1 } { - } R _ { 3 }$ cannot affect the liveness of objects in $R _ { 4 }$ (this is also true for $R _ { 5 }$ and $R _ { 6 }$ as they are closed). Thus, we can safely ignore references to objects in suspended regions when managing liveness. For example, if objects in $R _ { 1 } , R _ { 2 }$ or $R _ { 3 }$ are managed by reference counting, we do not need to increment or decrement reference counts when manipulating paused references in $R _ { 4 } .$ . Similarly, if objects in $R _ { 1 } , R _ { 2 }$ or $R _ { 3 }$ are managed by a tracing GC, tracing in $R _ { 4 }$ does not need to follow paused references. Thus, when managing memory in the active region, we can safely ignore any outgoing references, and so it is possible for $R _ { 1 } { - } R _ { 3 }$ to use different strategies, unbeknownst to $R _ { 4 }$ and irrespective of how $R _ { 4 }$ manages its memory (G2). The only references to objects in $R _ { 5 }$ and $R _ { 6 }$ that are possible (given that the regions are closed) are to the bridge objects ?? and ??. Aliases of ?? are not possible in $R _ { 4 }$ as iso references are unique, and we cannot transfer references out of an immutable ??. However, we do need to track liveness of the reference to ??, which is possible to do statically $e . g .$ , as in Rust. 

From the reasoning above it follows that liveness of objects in $R _ { 4 }$ can be determined by looking at objects in $R _ { 4 }$ alone, meaning that the costs of memory management are determined by the contents of and activity in $R _ { 4 } ~ ( \mathrm { G 1 } )$ . This makes it possible to collect garbage in just $R _ { 4 }$ (G3). 

# 6 PROGRAMMING WITH REGGIO REGIONS

As an example of how regions enable predictable memory management performance, consider the server application $^ { \mathrm { * } } P o ^ { \mathrm { * } }$ with the following key characteristics (see Listing 3 for skeleton code): 

(1) The server serves incoming requests. Tasks that process requests are short-lived and their side-effects are typically in the form of data stored in a database. 

(2) Responses to requests are served from data in an in-memory key–value store implemented as a skip list. This storage will shrink and grow continuously during execution. 

(3) Values in the store can have a complicated graph-like structure (e.g., they may contain cycles). 

We now explain how we can express this, and compare with Cyclone, MLKit, Pony, and Rust. 

Processing Requests. To manage allocations necessary to process a request, each request is wrapped in a region (Line 16, Line 49). These regions use arena allocation: allocations in the region persist until the region itself is deallocated. This gives cheap bump-pointer allocation and fast deallocation of the entire region once the response has been computed. If a request must be processed by different threads, this can be done cheaply due to the transferability of iso’s. If some requests turn out to require considerable processing time, their corresponding regions might switch away from arena allocation at the small cost of changing one annotation at the creation site of the region(s). 

Comparison. Cyclone’s LIFO regions are perfect for this purpose (as are MLKit’s, provided that the inference engine infers according to intentions, or better). While Cyclone does not support switching from arena allocation, it does permit manually managed reference counts or unique objects that can be manually deallocated during the arena’s lifetime. Pony only supports memory management on a per-actor level, (using tracing GC). We could make each request an actor, but it would have to communicate asynchronously with all surrounding state. Rust does not support regions, but could e.g., build a unique object holding unique values and thread this object through computation. While more complicated, Rust’s values would have individual lifetimes. 

Key–Value Store. The key–value store is implemented as a single region containing a skip list (Line 5, could as well be a hash table, B-tree, etc.). As it is a large long-living data structure of objects with different lifetimes, arena allocation is not a suitable strategy. Furthermore, if the resources (values) stored in the skip list are costly, reference counting is a good choice as it allows the resources in the list to be recycled immediately when they become garbage. Alternatively (the path we chose) is to make each element a region of its own, with independently managed memory (Line 1). 

If the store is large enough to warrant parallel accesses, it can be divided into several smaller regions with one list each. For a compile-time guarantee that no reference count manipulations in the skip list are needed during lookups, lookups can be implemented using explore rather than enter as the former opens the skip list region directly as paused (Line 23, and its dynamic extent foo()). Note that since the elements in the skip list are regions of their own, these can be entered separately and thus mutated, even if the list structure is immutable (Line 35). 

Comparison. Cyclone’s dynamic regions are a good fit for the key–value store. The objects that make up the skip list would require manual reference counting, which can be laborious. Failure to properly manage reference counts in Cyclone will also lead to “memory leaks” which will not be reclaimed until the region holding the store is (manually) destroyed. Pony can wrap the skip list inside an actor with an asynchronous interface, and manage its memory using tracing of the entire list leading to more time spent tracing memory and more floating garbage. Skip lists (hash tables, B-trees, etc.) cannot be constructed in Rust without judicious use of unsafe. Safe Rust’s reference counting does not relax its ownership rules, so mutation of aliased values is not permitted. 

Values in the Store. Finally, the elements in the store are suitable for either reference counting or tracing GC because of their graph-like structure (Line 42 delegates this decision to a factory method). GC is especially favoured in the (possible) presence of cycles which are expensive to detect with reference counting [Jones et al. 2016]. 

Comparison. Later versions of Cyclone and MLKit support a global arena where memory is managed using tracing GC. Thus, all elements in the store contribute to pressure on the same GC, and GC requires tracing through all elements to free garbage objects in one element. MLKit’s region propagation requires all elements to be put in a single region. Pony can handle this pattern by making each element an actor, which makes each element aliasable, and use an asynchronous interface. Finally, Rust will not be able to express and manage lifetimes of these structures automatically. A combination of unsafe and manual memory management is needed. 

Navigating Regions. Listing 4 shows a zip computation involving three unrelated regions. 

Using explore, we open the staff and reviews regions to make them temporarily immutable and their contents accessible. Finally, we open the zip region using enter. This makes the region active which allows allocation of the two iterators on Lines 8 and 9 and any allocation needed by the call to append on Line 11 to extend the list. It also allows the mutation in the next() calls to advance the iterators, and the mutation necessary to add the new pair to the zip list. Allocating the iterators inside the same region as their corresponding list is not useful as it would make the iterators immutable on Lines 11 and 12. This would cause the program to not typecheck—as the next() method needs to update the iterator, it needs to be called on a mut or tmp receiver. Since the iterators need to store paused references, they must be tmp (c.f., Table 4). This can be handled in iterator() by overloading on the self type, letting the implementation with paused self-type return a tmp reference. Elements are immutable so e.g., next() returns imm. 

let staff : iso List[imm Employee] = ...
let reviews : iso List[imm Review] = ...
var zip = new iso List[imm (String, Int)]
explore staff { s => // open as immutable
    explore reviews { r => // open as immutable
    enter zip { z => // open as mutable
    let si = s.iterator() // si is tmp
    let ri = r.iterator() // ri is tmp
    while (si.has_more() && ri.has_more()) {
    z.append(new imm Pair(si.next().name(), ri.next().calculate_salary()))
    }} 

Design Thoughts on Explore vs. Enter—And Invariants. The explore construct is essentially syntactic sugar for two nested enter blocks, the outermost entering the region to be explored and the innermost entering a fresh region: 

explore x { y => . . . } desugars to enter x { y => { enter (new iso Unit) { _ => . . . } } 

```rust
type_alias KV = Skiplist[imm Id, iso Value] // To shorten code horisontally for this presentation
type_alias Response = UpdateOK | InsertOK | DeleteOK | Failure

def start_po(fn : imm String, server_socket : iso ServerSocket) : Unit {
    let kv : iso KV = new iso<RC> KV // create empty key-value store; <RC>=reference counting, see §8.3
    enter kv { list => // Populate key-value store from persistent storage
    let data : tmp File = open(fn, "r")
    ... list.insert(...) ... // read contents from data and add to list
} // data goes out of scope, so get's free'd and closed

    enter server_socket { ss =>
    while (ss.is_open()) {
    let socket : mut Socket = ss.accept() // new connection
    let raw_request : imm String = socket.read_request() // get incoming request

    let r : iso Response = enter new iso<Arena> Unit { _ => // arena-managed region for tmp allocations
    let work : mut List[mut Tasks] = RequestParser::parse(raw_request) // parse request
    let response = new mut List[mut Response] // holds all responses to tasks in request

    while (!work.empty()) {
    response.append(merge match work.pop() { // merges iso result from match into r since append expects mut
    case mut StopTask => return // stop service, no response to client
    case mut UpdateTask(id) => explore kv { kv' => update(kv', id) }
    case mut InsertTask(id, payload) => enter kv { kv' => insert(kv', id, payload) }
    case mut DeleteTask(id) => enter kv { kv' => delete(kv', id) }
    }
    responseaccumulate(new iso Message) // chained through accumulator, moves out of arena
} // arena effectively goes out of scope, allocs on line 17, 18 + any tmp objects are freed
socket.respond(r); // render response object
}}

def update(kv : paused KV, id : imm Id) : iso Response // process UpdateTasks, inside suspended kv
    let value : paused Store[iso Value] = kv.get(id) // reference to a link's reference to a Value
    enter *value { v => // Requires a dynamic check - because of aliasing cannot rule out v is already open
    v.add_log_entry() // adds surviving object to v's region
    v.remove_some_token() // makes object in v's region garbage
    return new iso UpdateOK(v) // moves out of *value and kv regions and merged into r on line 21
}

def insert(kv : mut KV, id : imm Id, payload : imm Payload) : iso Response // process InsertTasks, inside kv
    let new_value = Factory::create(id, payload) // decides memory management for new_value dynamically
    enter new_value { v => v.tokens = new mut List; v.log = new mut List }
    kv.insert(id, new_value) // buries new_value
    return new iso InsertOK() // moves out of kv region and merged into r on line 21
}

def delete(kv : mut KV, id : imm Id) : iso Response // process DeleteTasks, inside kv
    enter new iso<Arena> Unit { _ => // create new tmp region, the one created on Line 16 is not accessible here
    let q : mut SQL_Query = Factory::make_can_delete_query(id) // q cannot refer to kv because it is mut
    let r : tmp SQL_Result = Backend::execute_query(q) // because it is tmp, r could refer to kv if it needed to
    if (!r.OK) return new iso Failure(...) // moves out of tmp and kv regions and merged into r on line 21
} // arena goes out of scope, allocs on lines 50, 51 are free'd
return enter kv.remove(id) { v => new iso DeleteOK(v) } // Creates garbage in kv, drops a Value region
} 
```

Listing 3. Skeleton code for Po. To save space, we permit constructor arguments to iso objects to take mut arguments (Lines 38, 45, 52, and 54). This is safe and can desugar to an extra enter. These lines all create an object that escapes the active region and is merged into the enclosing region, making them mut in r (append expects a mut argument). The ServerSocket is created elsewhere. It likely does not use arena allocation since it allocates on each turn of the loop (Line 13). If it did, those allocations would only be free’d on Line 31. The default annotation on new iso is <Arena> (c.f., §8.3). Notice how code is agnostic to how memory is managed. 

The first enter activates the region, and the second suspends it. The new region (new iso Unit) is independent from the rest of the program. As it is active, it serves all allocations that appear inside the explore block (as suspended regions do not permit allocation or deallocation, Table 1). What explore guarantees that unprincipled nesting of enters does not, is that the explored region was not mutated before suspended. Conceptually, this is a big difference as we will explain next. 

Similar to object invariants, we expect invariants of a closed region to hold at the time of opening. While active, invariants may temporarily be broken and then restablished before the region is closed. As a nested enter can reference any enclosing region, it will be able to observe any invariants broken by mutation following the opening of the enclosing regions. By opening regions directly in a suspended state, explore ensures that region invariants continue to hold. We are considering using a separate capability to capture this statically. We are also considering an “eager” explore construct that opens a region along with all its subregions as suspended in one fell swoop. This would avoid the need for explicit opening of subregions thus further simplifying working with immutable objects. The cost is more complexity in the type system. Time will tell whether this complexity is warranted or not. 

With respect to memory management, explore allows opening a region for reading, and navigating through it, without any memory management overhead as the region’s object structure is invariant and there are neither allocations nor deallocations in the region. 

Reggio’s Borrowing Capabilities. Traditional borrowing as originally introduced by Wadler [1990], explored deeply by e.g., Boyland [2001] and Boyland et al. [2001], and popularised by Rust relaxes uniqueness of a value in a well-defined lexical scope. We can express a similar form of borrowing through the 

```txt
var x = new iso Cell // x : var Store[iso Cell]
enter y { => _ // now x : paused Store[iso Cell]
    enter *x { => z
    ... // Can still mutate z!
    }
} 
```

type paused Store[iso T], i.e., a reference to a storage location in a suspended region storing a reference to a closed region. Such a reference (e.g., x) can be shared freely inside a single thread, allowing it to flow to a place where it can be opened (enter *x) with mutation rights, including swapping the bridge object (as long as the new bridge object is a subtype of T). 

# 7 FORMALISING REGGIO

We formalise Reggio through two interacting languages: region and command, and their respective semantics. The former controls all accesses to memory (loads and stores), allocation of objects in regions, creating, merging, freezing—and importantly entering and exiting—regions. The most important properties of the region language is expressed as a topology invariant (c.f., §7.5). The command language is essentially “what the programmer wrote”. This separation makes it possible to specify e.g., under what conditions a store is valid, irrespective of what caused the store. 

During execution, the command language emits effects which the region language performs. The static semantics of the command language ensures, modulo one dynamic check, that the topology invariant is preserved. We present most of the rules of the region language and key type system rules. Additional details are available in the appendix. 

# 7.1 Dynamic Semantics of the Region Language

A configuration in the region language has four components: a LIFO region stack RS and the sub-heaps of open regions $H _ { o p } ,$ closed regions $H _ { c l }$ , and frozen regions $H _ { f r }$ The latter is an unimportant simplification (conceptually, only mutable objects live in regions). A heap ?? is a collection of disjoint regions ??. Opening a closed region moves it from $H _ { c l }$ 

$$
r c f g \quad : := \quad \langle R S; H; H; H \rangle
$$

$$
R S \quad : := \quad R F:: R S \mid \epsilon
$$

$$
R F \quad : := \quad (r, S, F)
$$

$$
H \quad : := \quad R \mid H * H \mid \epsilon
$$

Fig. 6. Configuration in region language (1/2) 

to $H _ { o p }$ and pushes a new stack frame on top of RS. Closing the top-most region in RS returns it back to $H _ { c l }$ . Freezing a region moves $\operatorname { i t } ,$ and all regions reachable from it, permanently to $H _ { f r }$ . As an example, consider the region topology depicted in Fig. 5. We can write down the corresponding configuration as ⟨RS, $H _ { o p } , H _ { c l } , H _ { f r } \rangle$ where 

$$
\begin{array}{l} R S = R F _ {4}:: R F _ {3}:: R F _ {2}:: R F _ {1}:: \epsilon \\ H _ {o p} = R _ {1} * R _ {2} * R _ {3} * R _ {4} \\ H _ {c l} = R _ {5} * R _ {6} \\ H _ {f r} = \epsilon \\ \end{array}
$$

For the region sub-heap $R _ { i } ,$ if $R _ { i }$ is open (i.e., part of $H _ { o p } )$ , $R F _ { i }$ is its region frame (depicted in Fig. 5 as a white box) that holds the stack variables created in the scope of the corresponding enter block. Opening the closed region $R _ { 6 }$ would push a new frame $R F _ { 6 }$ above $R F _ { 4 }$ in RS, and move $R _ { 6 }$ from $H _ { c l }$ to $H _ { o p }$ . Similarly we could freeze (merge) $R _ { 6 }$ , which would move it from $H _ { c l }$ to $H _ { f r }$ (remove it from $H _ { c l }$ and merge it into $R _ { 4 } )$ . 

In this model, an inter-region reference into an open region is permissible iff it points downwards in the region stack (from left to right according to RS), or it is the unique (iso) reference through which the region was opened. The LIFO region stack constitutes a “path” through the region forest that corresponds to the opening order of enter blocks $( c . f . ,$ the region LIFO order in Fig. 5). Thus, in Fig. 5, any reference from $R _ { 4 }$ to $R _ { 3 }$ is permissible (as long as we do not close $R _ { 4 } )$ , while a reference from $R _ { 2 }$ to $R _ { 3 }$ must necessarily be the reference from object ?? to object ?? . We model explore as nested enters. 

$$
\begin{array}{l} S \quad : := \quad \iota \mapsto o, S \mid \epsilon \\ F \quad : := \quad f \mapsto v?, F \mid \epsilon \\ v? \quad : := \quad v \mid \mathbf {u n d e f} \\ v \quad : := \quad (k, \iota) \\ R \quad : := \quad (r, S) \\ o \quad : := \quad (\# C L, F) \\ \end{array}
$$

Fig. 7. Configuration in region language (2/2) 

A region stack frame $R F$ contains a region identifier ?? , a temporary store ?? for objects whose lifetimes are bounded by the scope of the region’s enter block (values with capability tmp), and a map ?? from variable names $f$ to values ??, representing the local variables in that enter block (we model destructive reads of a variable ?? by remapping it to undef, at which point reading ?? again will lead to the program getting stuck). A region ?? is a tuple of a (unique) region identifier ?? and a store ?? containing the objects in that region. 

Objects are identified by ??. Values ?? are object identifiers ?? tagged with a capability ??. Stores ?? map object ids ?? to objects ?? which store their class tag #???? and fields (for simplicity we reuse the same ?? as for local variables, although a field will never contain undef). 

The command language communicates with the region language via effects. The relation $r c f g \xrightarrow { \mathit { \Delta } E f f } r c f g ^ { \prime }$ should be understood as performing the effect $E \mathcal { f }$ in $r c f g$ , resulting in $r c f g ^ { \prime }$ . Effects include entering and exiting a region, loading a value from an object store, writing (swapping) a value for another in an object store, merging or freezing a region, etc. We now describe a selection of rules for these effects. 

$$
\begin{array}{c} \text {REGION - LOAD} \\ x f r e s h \qquad F (y) = (k, \iota) \\ \text {cfg\_load} ((r, S, F):: R S, H _ {o p} * H _ {f r}, \iota) = o [ f \mapsto v ] \\ F ^ {\prime} = F, x \mapsto (k \odot v) \\ \hline \langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {load} (x , y . f)} \langle (r, S, F ^ {\prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \end{array}
$$

In every effect, the first parameter should be understood as the name of the variable where the results should be stored. For example, the effect $l o a d ( x , y . f )$ is handled in rule region-load by binding the value of field $y . f$ to variable ??. First, the value of ?? is looked up in the top stack frame as (??, ??). The object id ?? is used to find the corresponding object ?? in the configuration—it may be stored in the subheap of an open or frozen region, or in one of the temporary stores on the region stack. The capability ?? is used for viewpoint adaptation of the (capability of the) value ?? of field ?? in ?? before it is inserted into the top frame. 


region-swap-temp


<table><tr><td rowspan="2"><eq>x</eq> fresh</td><td><eq>\mathbf{g}\mathbf{e}\mathbf{t}(F, use) = (v, F&#x27;)</eq></td><td><eq>F&#x27;(y) = (k, \iota)</eq></td><td><eq>\text{load}(S, \iota) = o[f \mapsto v&#x27;]</eq></td></tr><tr><td><eq>o&#x27; = o[f \mapsto v]</eq></td><td><eq>\text{store}(S, \iota, o&#x27;) = S&#x27;</eq></td><td><eq>F&#x27;&#x27; = F&#x27;, x \mapsto v&#x27;</eq></td></tr><tr><td colspan="4"><eq>\langle (r, S, F) :: RS; H_{op}; H_{cl}; H_{fr} \rangle \xrightarrow{\text{swap}(x,y.f,use)} \langle (r, S&#x27;, F&#x27;&#x27;&#x27;) :: RS; H_{op}; H_{cl}; H_{fr} \rangle</eq></td></tr></table>


region-swap-heap


<table><tr><td rowspan="2">x fresh</td><td><eq>\mathbf{get}(F, use) = (v, F&#x27;)</eq></td><td><eq>F&#x27;(y) = (k, \iota)</eq></td><td><eq>\text{load}(S&#x27;, \iota) = o[f \mapsto v&#x27;]</eq></td></tr><tr><td><eq>o&#x27; = o[f \mapsto v]</eq></td><td><eq>\text{store}(S&#x27;, \iota, o&#x27;) = S&#x27;&#x27;</eq></td><td><eq>F&#x27;&#x27; = F&#x27;, x \mapsto v&#x27;</eq></td></tr><tr><td colspan="4"><eq>\langle (r, S, F) :: RS; (r, S&#x27;) * H_{op}; H_{cl}; H_{fr} \rangle \xrightarrow{\text{swap}(x,y.f,use)} \langle (r, S&#x27;, F&#x27;&#x27;) :: RS; (r, S&#x27;&#x27;) * H_{op}; H_{cl}; H_{fr} \rangle</eq></td></tr></table>

Field assignments are caused by the effect swap(??, ??.?? , use), which writes the value of use to $y . f$ and binds the old value of $y . f$ to ?? . A use is a potentially destructive variable access (?? or drop ??, see Fig. 9). Rules region-swap-temp and region-swap-heap handle the cases where the object being assigned to is in the temporary store or on the heap. In both cases, we perform the use (which may make a variable invalid) with the helper function get (see Fig. 8). We then proceed just as when loading a field, but finish by updating get(?? [?? ↦→ ??], drop ??) = 

(??, ?? [?? ↦→ undef]) 

get(?? [?? ↦→ (??, ??)], ?? ) = 

$( ( k , \iota ) , F [ x \mapsto ( k , \iota ) ] )$ 

if ?? ≠ var ∧ ?? ≠ iso 

Fig. 8. (Non-)destructive reads 

the object being assigned to and update its containing store ??. Note that assigning and loading mutable variables are special cases of the swap and load effects since we model mutable variables as single-field objects. 


region-alloc-heap-mut


<table><tr><td>x fresh</td><td>∀i ∈ [1, n]. <eq>\mathbf{get}\left(F_i, use_i\right) = (v_i, F_{i+1})</eq></td><td><eq>\mathbf{fields}\left(C\right) = f_1, ..., f_n</eq></td></tr><tr><td>o = (\#C, [f1 ↦ v1, ..., fn ↦ vn])</td><td><eq>\iota</eq> fresh</td><td><eq>S&#x27;&#x27; = S&#x27;, \iota \mapsto o</eq> <eq>F&#x27; = F_{n+1}, x \mapsto (\text{mut}, \iota)</eq></td></tr><tr><td><eq>\langle (r, S, F_1) :: RS; (r, S&#x27;) * H_{op}; H_{cl}; H_{fr} \rangle</eq></td><td><eq>\xrightarrow{\text{halloc}(x, \text{mut}, \#C, use_1 ... use_n)}</eq></td><td><eq>\langle (r, S, F&#x27;) :: RS; (r, S&#x27;&#x27;) * H_{op}; H_{cl}; H_{fr} \rangle</eq></td></tr></table>


region-alloc-heap-iso


<table><tr><td colspan="2"><eq>x</eq> fresh <eq>\forall i \in [1, n]. \mathbf{get}(F_i, use_i) = (v_i, F_{i+1})</eq> <eq>\mathbf{fields}(C) = f_1, ..., f_n</eq><eq>o = (\#C, [f_1 \mapsto v_1, ..., f_n \mapsto v_n])</eq> <eq>\iota</eq> fresh <eq>r&#x27;</eq> fresh <eq>F&#x27; = F_{n+1}, x \mapsto (\text{iso}, \iota)</eq></td></tr><tr><td colspan="2"><eq>\langle (r, S, F_1) :: RS; H_{op}; H_{cl}; H_{fr} \rangle \xrightarrow{\text{halloc}(x, \text{iso}, \#C, use_1 ... use_n)} \langle (r, S, F&#x27;) :: RS; H_{op}; (r&#x27;, [\iota \mapsto o]) * H_{cl}; H_{fr} \rangle</eq></td></tr></table>

Allocation on the heap is caused by the effect $h a l l o c ( x , k , C , u s e _ { 1 } . . . u s e _ { n } )$ , which instructs the region language to heap allocate a new ?? object with fields initialized according to $u s e _ { 1 } . . . u s e _ { n }$ and bind it to the name ??. The capability ?? denotes whether to allocate in the current region (region-allocheap-mut) or in a new region (region-alloc-heap-iso). Since each use is a possibly destructive variable access the ordering matters. We begin by performing these one by one with the local variables $F _ { 1 }$ . Each value $v _ { i }$ is paired up with the corresponding field $f _ { i }$ of the class and put into an object ??. We then add ?? at location ?? to the subheap of the currently active region, or add a new region $r ^ { \prime }$ in the closed regions containing only the object ?? at location ??. Finally we bind the object to ?? in the top frame with capability mut or iso. We omit the rule region-alloc-temp which allocates objects with capabilities tmp or var in the temporary store ?? of the currently active region frame. 

The key rules of the region language govern entering and exiting a region. 

# region-enter-ok

$$
\begin{array}{c} w f r e s h \qquad \forall i \in [ 1, n ]. z _ {i} f r e s h \qquad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = ((k _ {i}, \iota_ {i}), F _ {i + 1}) \\ \qquad \qquad \qquad \forall i \in [ 1, n ]. v _ {i} ^ {\prime} = \left\{ \begin{array}{l l} (k _ {i}, \iota_ {i}) & \text {if k_{i} = iso} \\ (\text {paused} \odot k _ {i}, \iota_ {i}) & \text {otherwise} \end{array} \right. \\ F = [ z _ {i} \mapsto v _ {i} ^ {\prime} | i \in [ 1, n ] ] \qquad F _ {n + 1} (y) = (_ {-}, \iota) \qquad \text {cfg\_load} ((r, S, F _ {1}):: R S, H _ {o p}, \iota) = o [ f \mapsto (_ {-}, \iota^ {\prime}) ] \\ \iota^ {\prime} \in d o m (S ^ {\prime}) \qquad \iota^ {\prime \prime} f r e s h \qquad F ^ {\prime} = F, w \mapsto (k, \iota^ {\prime \prime}) \qquad R F = (r ^ {\prime}, [ \iota^ {\prime \prime} \mapsto (\# C e l l, [ v a l \mapsto (m u t, \iota^ {\prime}) ]) ], F ^ {\prime}) \\ \hline \langle (r, S, F _ {1}):: R S; H _ {o p}; (r ^ {\prime}, S ^ {\prime}) * H _ {c l}; H _ {f r} \rangle   \xrightarrow {\text {enter} (w , k , y . f , z _ {1} = u s e _ {1} , . . . , z _ {n} = u s e _ {n})}   \langle R F:: (r, S, F _ {n + 1}):: R S; (r ^ {\prime}, S ^ {\prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \end{array}
$$

# region-enter-fail

$$
\frac {\forall i \in [ 1, n ] . \mathbf {g e t} (F _ {i} , u s e _ {i}) = ((k _ {i} , \iota_ {i}), F _ {i + 1}) \qquad F _ {n + 1} (y) = (\_, \iota)}{\text { cfg\_load } ((r , S , F) : : R S , H _ {o p} , \iota) = o [ f \mapsto (\_, ^ {\prime}) ] \qquad \forall R ^ {\prime} \in H _ {c l} .   \iota^ {\prime} \notin d o m (R ^ {\prime}. S)}
$$

region-enter-ok shows successfully entering a region $r ^ { \prime }$ through its bridge object $\iota ^ { \prime }$ stored in the field ?? of the variable ??. (This operation can fail if $R ^ { \prime }$ is already opened. This will not change the state in the region language, as seen in region-enter-fail, and it is up to the command language to choose how to handle this: by exception, having a construct like if-enter-else, etc. For simplicity, the command language steps to a failure state.) The enter effect supplies four things: the name ?? and capability $k$ of the parameter of the enter block, the field $y . f$ through which we are entering, and a list of bindings $\overline { { z = u s e } }$ denoting the block’s captured variables. Going back to Listing 1, the enter block captures i, so the corresponding effect would include $z = \mathrm { i }$ . Note that ?? is chosen by the command language and due to variable renaming is not necessarily i. Considering the rule again, we first use the get helper function to perform the uses. For each resulting value $( k _ { i } , \iota _ { i } )$ , we apply the paused viewpoint adaptation when $k _ { i }$ is not iso and create a new mapping ?? of the captured variables. We then get the value ?? of $y ,$ load its corresponding object ?? and extract the value $\iota ^ { \prime }$ of field $f$ (our bridge object). On the last line of the premises we check that $\iota ^ { \prime }$ is an object in a closed region $r ^ { \prime } ;$ this region will be moved into the collection of open regions. We extend ?? with a mapping from ?? to a fresh location $\iota ^ { \prime \prime }$ , and finally install this extended $F$ into a region frame RF where $\iota ^ { \prime \prime }$ is the identifier of a ref cell object pointing to our bridge object $\iota ^ { \prime } .$ . We push this region frame onto the region stack. 

# region-exit-heap

$$
\begin{array}{c} x   f r e s h \quad \mathbf {g e t}   (F ^ {\prime},   u s e) = (v, F ^ {\prime \prime}) \qquad F ^ {\prime \prime} (z) = (_ {-}, \iota^ {\prime}) \\ \text {load} (S ^ {\prime}, \iota^ {\prime}) = o ^ {\prime} [ f ^ {\prime} \mapsto (_ {-}, \iota^ {\prime \prime}) ] \\ F (y) = (_ {-}, \iota) \qquad \text {heap\_load} (H _ {o p}, \iota) = o [ f \mapsto (k, _ {-}) ] \\ \text {heap\_store} (H _ {o p}, \iota , o [ f \mapsto (k, \iota^ {\prime \prime}) ]) = H _ {o p} ^ {\prime} \qquad F ^ {\prime \prime \prime} = F, x \mapsto v \\ \hline \langle (r ^ {\prime}, S ^ {\prime}, F ^ {\prime}):: (r, S, F):: R S; (r ^ {\prime}, S _ {o p}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {e x i t (x , u s e , y . f , z . f ^ {\prime})} \langle (r, S, F ^ {\prime \prime \prime}):: R S; H _ {o p} ^ {\prime}: (r ^ {\prime}, S _ {o p}) * H _ {c l}; H _ {f r} \rangle \end{array}
$$

# region-exit-temp

$$
\begin{array}{c} x f r e s h \quad \mathbf {g e t} (F ^ {\prime}, u s e) = (v, F ^ {\prime \prime}) \qquad F ^ {\prime \prime} (z) = (_ {-}, \iota^ {\prime}) \\ \text {load} (S ^ {\prime}, \iota^ {\prime}) = o ^ {\prime} [ f ^ {\prime} \mapsto (_ {-}, \iota^ {\prime \prime}) ] \\ F (y) = (_ {-}, \iota) \qquad \text {stack\_load} ((r, S, F):: R S, \iota) = o [ f \mapsto (k, _ {-}) ] \\ \text {stack\_store} ((r, S, F):: R S, \iota , o [ f \mapsto (k, \iota^ {\prime \prime}) ]) = (r, S ^ {\prime \prime}, F):: R S ^ {\prime} \qquad F ^ {\prime \prime \prime} = F, x \mapsto v \\ \hline \langle (r ^ {\prime}, S ^ {\prime}, F ^ {\prime}):: (r, S, F):: R S; (r, S _ {o p}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {e x i t (x , u s e , y . f , z . f ^ {\prime})} \langle (r, S ^ {\prime \prime}, F ^ {\prime \prime \prime}):: R S ^ {\prime}; H _ {o p}; (r, S _ {o p}) * H _ {c l}; H _ {f r} \rangle \end{array}
$$

region-exit-heap and region-exit-temp describe exiting the region $r ^ { \prime } { } _ { ; }$ , popping its region frame from the top of the region frame stack. After exiting, the region frame corresponding to ?? will be on the top of the stack and thus active. The exit effect provides use and ?? which correspond to the return value and the variable to which this will be bound (in the stack frame of $r ) . z . f ^ { \prime }$ specifies a location in $r ^ { \prime }$ where a reference to the new bridge object can be found. Finally, $y . f$ specifies a location where this reference will be written. The only difference between region-exit-heap and region-exit-temp is where the object pointed to by ?? is located. In the former it is on the heap of some open region, while for the latter it is in a temporary store in the region stack. 

For simplicity, we do not implicitly reinstate iso variables captured from the previous region even if they are still valid upon exit from a region (this would be sound, $c . f . ,$ , Listing 4 where variables reviews and zip are reinstated in the top-level scope after line 13). This is without loss of generality as we can return them in an object together with the result ?? and reinstate them manually. 

REGION-FREEZE
x fresh $\mathbf{g}\mathbf{e}\mathbf{t}(F, use) = ((k, \iota), F')$ $\iota \in dom(R.S)$ $H = reachable\_regions(R, (H * H_{cl}) * H_{op})$ $F'' = F', x \mapsto (\text{imm}, \iota)$ $\langle(r, S, F) :: RS; H_{op}; R * (H * H_{cl}); H_{fr} \rangle \xrightarrow{\text{freeze}(x, use)} \langle(r, S, F'') :: RS; H_{op}; H_{cl}; (R * H) * H_{fr} \rangle$ 

REGION-MERGE
x fresh $\mathbf{g}\mathbf{e}\mathbf{t}(F, use) = ((k, \iota), F') \quad \iota \in dom(R.S)$ $R' = (r, S' \uplus R.S) \quad F'' = F', x \mapsto (\text{mut}, \iota)$ $\langle(r, S, F)::RS; (r, S') * H_{op}; R * H_{cl}; H_{fr} \rangle \xrightarrow{\text{merge}(x, use)} \langle(r, S, F'') :: RS; R' * H_{op}; H_{cl}; H_{fr} \rangle$ 

The rules for merging (region-merge) and freezing (region-freeze) are similar. Both perform a use to get an object identifier ?? and find its containing region ?? among the closed regions. For merges, the subheap of ?? is merged with the subheap of the currently active region, and ?? is bound to ?? as mut. For freezes, all the reachable regions of ?? are moved from the closed to the frozen regions together with ??, and ?? is bound to ?? as imm. 

In addition to allocation in the temporary store, we have omitted the rules for type casts and rebinding of variables. 

# 7.2 Static Semantics of the Command Language

The command language is an imperative language in A-normal form. The syntax is shown in Fig. 9. We encode mutable variables of type ?? as ref cells. For uniformity we model these as objects of type Cell[??] with a single field val. The if typetest expression is a dynamic type test similar to Java 16 style pattern matching, drop ?? denotes a destructive read, *lval dereferences a field or ref cell, and var allocates a new ref cell with the capability var and initializes its 

e ::= use | let x = b in e
    | if typetest(use, t) { y => e } { y => e }
use ::= x | drop x
b ::= *lval | lval := use | fnc( $\overline{use}$ ) | var use
    | new k C( $\overline{use}$ ) | freeze use | merge use
    | enter lval [ $\overline{y = use}$ ] { z => e } | e
lval ::= x | x.f
t ::= k CL | t | t 


Fig. 9. Syntax of the command language


value from use. For simplicity we provide enter blocks with an explicit capture list, but this could also be inferred from variable use. Types ?? are unions $t _ { 1 } \mid t _ { 2 }$ or ?? ????, where ?? is a capability and ???? is Cell or a class name ??. 

The static semantics is a flow-sensitive type system producing judgements of the form $\Gamma _ { 1 } \vdash r : t \ \lnot \ \Gamma _ { 2 } \left( r \in e \cup b \cup u s e \right)$ . Thus it statically tracks destructive reads and strong updates of unique variables. We discuss a few of the rules below. 

CMD-TY-USE-KEEP $\vdash \Gamma_{1} \quad \Gamma_{1}(x) = k CL$ $k \neq iso \quad k \neq var$ $\Gamma_{1} \vdash x : k CL \dashv \Gamma_{1}$ 

CMD-TY-USE-DROP $\vdash \Gamma_{1}$ $\Gamma_{1} = \Gamma[x : t]$ $\Gamma_{2} = \Gamma[x : \mathbf{undef}]$ $\overline{\Gamma_{1} \vdash \mathbf{drop} x : t \vdash \Gamma_{2}}$ 

CMD-TY-DEREF-FIELD $\Gamma(x) = k CL$ $\mathbf{ftype}(CL, f) = t$ $\vdash k \odot t$ $\overline{\Gamma \vdash *x.f : (k \odot t) + \Gamma}$ 

cmd-ty-assign 

$$
\Gamma_ {1} \vdash u s e: t \dashv \Gamma_ {2} \quad \Gamma_ {2} (x) = k C L
$$

$$
\mathbf {f t y p e} (C L, f) = t \quad k \in \{\text { mut }, \text { tmp } \}
$$

$$
\Gamma_ {1} \vdash x. f := u s e: t \dashv \Gamma_ {2}
$$

cmd-ty-assign-var 

$$
\Gamma_ {1} \vdash u s e: t _ {1} \dashv \Gamma_ {2} [ x: \operatorname{varCell} [ t _ {2} ] ]
$$

$$
\Gamma_ {1} \vdash x := u s e: t _ {2} \dashv \Gamma_ {2} [ x: \operatorname{varCell} [ t _ {1} ] ]
$$

Reading a variable ?? that is not a var or iso is straightforward and introduces an alias (cmd-tyuse-keep). When ?? is var or iso, cmd-ty-use-drop allows reading the variable but undefines it in the environment as a side-effect to ensure its single use. When accessing a field $x . f$ (cmd-tyderef-field), its type is subject to viewpoint adaptation ?? ⊙ ?? where ?? is the capability of ?? and ?? the type of $f .$ . Note that viewpoint adaptation disallows reading an iso field unless ?? is imm, expressing the fact that freezing a region is deep (all nested regions will be frozen as well). A field $x . f$ can be updated through assignment (cmd-ty-assign) when the capability of ?? is mut or tmp, i.e., internal references in the currently active region (note that assignment returns the old value of the field). Viewpoint adaptation is not needed as we are moving values rather than copying them, allowing swapping of iso references. Local variables allow strong updates (cmd-ty-assign-var). As we model them as ref cells we update the type parameter for ?? after assignment. 

cmd-ty-enter 

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1} \quad \Gamma_ {n + 1} (x) = k C L
$$

$$
\operatorname{open} (k) \quad \text { f   t   y   p   e } (C L, f) = t \quad \operatorname{cap} (\operatorname{iso}, t)
$$

$$
\Gamma^ {\prime} = y _ {1}: t _ {1} ^ {\prime}, \dots , y _ {n}: t _ {n} ^ {\prime} \text {   where   } t _ {i} ^ {\prime} = \left\{ \begin{array}{l l} t _ {i} & \text { if   } \operatorname{cap} (\operatorname{iso}, t _ {i}) \\ \text { paused } \odot t _ {i} & \text { otherwise } \end{array} \right.
$$

$$
t ^ {\prime} = \text {make\_mut} (t) \quad \Gamma^ {\prime}, z: \operatorname{tmpCell} [ t ^ {\prime} ] \vdash e: t ^ {\prime \prime} \dashv \Gamma^ {\prime \prime}, z: \operatorname{tmpCell} [ t ^ {\prime} ] \quad \text {cap} (\{\text {iso,imm} \}, t ^ {\prime \prime})
$$

$$
\Gamma_ {1} \vdash \text { enter } x. f [ y _ {1} = u s e _ {1}, \dots , y _ {n} = u s e _ {n} ] \{z = > e \}: t ^ {\prime \prime} + \Gamma_ {n + 1}
$$

cmd-ty-enter-var 

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1} \quad \Gamma [ x: \operatorname{varCell} [ t ] ] = \Gamma_ {n + 1} \quad \text { cap(iso,   } t)
$$

$$
\Gamma^ {\prime} = y _ {1}: t _ {1} ^ {\prime}, \dots , y _ {n}: t _ {n} ^ {\prime} \text {   where   } t _ {i} ^ {\prime} = \left\{ \begin{array}{l l} t _ {i} & \text { if   } \operatorname{cap} (\operatorname{iso}, t _ {i}) \\ \text { paused } \odot t _ {i} & \text { otherwise } \end{array} \right.
$$

$$
\Gamma^ {\prime}, z: \text { var   Cell } [ m a k e \_ m u t (t) ] \vdash e: t ^ {\prime \prime} \dashv \Gamma^ {\prime \prime}, z: \text { var   Cell } [ t ^ {\prime} ]
$$

$$
\operatorname{cap} \left(\text { mut }, t ^ {\prime}\right) \quad \operatorname{cap} \left(\{\text { iso }, \text { imm } \}, t ^ {\prime \prime}\right)
$$

$$
\Gamma_ {1} \vdash \text { enter } x [ y _ {1} = u s e _ {1}, \dots , y _ {n} = u s e _ {n} ] \{z = > e \}: t ^ {\prime \prime} \dashv \Gamma [ x: \text { var   Cell } [ m a k e \_ i s o (t ^ {\prime}) ] ]
$$

The predicate $\mathsf { c a p } ( k , t )$ asserts that the type ?? has capability $k ;$ the predicate open(??) is true if the capability denotes an open region, i.e., ?? is mut, tmp, var or paused. Finally, make_mut (??) and make_iso(??) return a ?? whose iso capabilities have been replaced by mut and vice versa. 

Opening a region through a field $x . f$ (cmd-ty-enter) requires that $x ' s$ capability is open, and $f ^ { \ast } s$ capability is iso. We create a new environment $\Gamma ^ { \prime }$ with the captured variables $y _ { 1 } , . . . , y _ { n }$ , using viewpoint adaptation to suspend the types of all non-iso variables, as well as a tmp ref cell holding the bridge object. We use make_mut (??) to change the type of the bridge from iso to mut as control is moving inside the opened region. Finally, the enter block may only return iso’s and imm’s. (Note that entering a region through a field incurs a dynamic check to see if the region is already open. ) 

Opening a region through a var ref cell (cmd-ty-enter-var) is similar to a field (cmd-ty-enter), but allows strong updates of the ref cell holding the bridge object by retaining its var capability. This allows changing the bridge object’s type from within the enter block. 

cmd-ty-merge 

$$
\Gamma_ {1} \vdash u s e: \text { iso } C L \dashv \Gamma_ {2}
$$

$$
\Gamma_ {1} \vdash \text { merge   use }: \text { mut } C L \dashv \Gamma_ {2}
$$

cmd-ty-freeze 

$$
\Gamma_ {1} \vdash u s e: \text { iso } C L \dashv \Gamma_ {2}
$$

$$
\Gamma_ {1} \vdash \text { freeze   use }: \text { imm } C L \dashv \Gamma_ {2}
$$

The rules for merging and freezing a region (cmd-ty-merge and cmd-ty-freeze) are straightforward. Both demand that the value that we operate on is an iso reference $( i . e . ,$ , bridge object to a closed region), and produce either a mut or imm depending on the operation. 

# 7.3 Dynamic Semantics of the Command Language

A configuration {????} in the command language is a dynamic expression ????, which is an extension of ?? by “entered blocks” that propagates syntactically the nesting structure of enters and exits, and thus dynamically tracks the nesting of open regions, and Failure, used to report failed dynamic checks when entering an already open region. The dynamic semantics steps a configuration and produces an effect of the same kind consumed by the region language. For example, the expression let $x = { \star } y . f$ in ?? produces the effect $l o a d ( x , y . f )$ , which tells the region language to load the field ?? from the object stored in ?? and store it in ??. 

# 7.4 Interaction Between the Region and Command Languages

A complete configuration is a product of the configurations of the region and command languages. It steps if there is an effect that steps both of them in tandem: 

$$
\begin{array}{l} \text { TANDEM - STEP } \\ \frac {d e \xrightarrow {E f f} d e ^ {\prime} \quad r c f g \xrightarrow {E f f} r c f g ^ {\prime}}{\langle \{d e \} r c f g \rangle \to \langle \{d e ^ {\prime} \} r c f g ^ {\prime} \rangle} \end{array}
$$

A dynamic expression is typed under a stack of typing contexts ${ \overline { { \Gamma } } } ,$ corresponding to the nesting of entered blocks. We lift the static semantics of the command language to define well-formed effects: the relation $\overline { { { \Gamma } } } \vdash E f f \ l { \ l { 1 } } \overline { { { \Gamma } } } ^ { \prime }$ statically describes the effect Eff and how it changes the typing context. For example, the static description of the effect $l o a d ( x , y . f )$ states that the type of ?? in the top-most entered block is ?? ????, that ???? has a field $f$ of type ?? , and that the viewpoint adapted type ?? ⊙ ?? is well-formed $( c . f . ,$ , cmd-ty-deref-field). 

In order to reason about soundness, we define well-formedness of a configuration in the region language as the relation $\overline { { \Gamma } } \vdash \langle R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ . A well-formed configuration ensures four things. First, the stack of environments $\overline { { \Gamma } }$ mirrors the region stack RS so that each environment describes the local variables of a region frame in RS. Second, each field of every object in the configuration contains a value that corresponds to its static type. Third, we have invariants about the reference capabilities: var references are unique, objects in frozen regions only refer to other references in frozen regions, mut references point within the same region and to the heap, tmp and var point within the same region and to the temporary store, paused references point downwards in the region stack, etc. Finally, we have the invariant that the object graph and its regions have the expected topology. We describe this invariant in detail in the following section. 

# 7.5 The Topology Invariant

The most important properties of the object (and region) graph are captured in a single invariant that we call the topology invariant (Fig. 10). We express this as a property that holds for any pair of references ref 1 and ref 2 in a well-formed configuration. The helper functions src() and dst() denote the storage location and referee of a reference respectively; reg() denotes the region of an object (or variable); regions() projects the region identifiers out of a set of regions ?? . 

For all references ref 1 and $r e f 2 ,$ , either: they are the same reference, e.g., both are stored in the same $\imath . f$ or variable $x \left( 1 \right)$ ; both refer to objects in different regions (2); or at least one of them is an intra-region reference (3); refers to a permanently immutable object (4); or is a reference outwards in the nesting hierarchy, downwards in the region stack (5). The relation $R S \vdash \operatorname { d s t } ( r e f ) \leq s \ r c ( r e f )$ 

holds if dst(ref ) is higher up in RS than src(ref ) and ref originates from the temporary store. In other words, we allow temporary references into suspended regions from open regions. 

The topology invariant has several important implications: The object graph inside a region is unconstrained (3). The object graph of the permanently immutable objects is unconstrained (4). Temporary objects in an open region ?? are allowed to refer to objects in an open region $R ^ { \prime }$ as long as $R ^ { \prime }$ was opened before ?? (5). Finally, considering the whole invariant, if we have two external references ((3) does not hold) pointing into the same non-frozen region ((2) and (4) do not hold), and neither of them points downwards in the region stack ((5) does not hold), then they ∀ref 1, ref 2 ∈ references(⟨RS; ??op; ??cl ; ??fr ⟩). 

$$
\bigvee \left\{ \begin{array}{l l} r e f 1 = r e f 2 & (1) \\ \operatorname{reg} (\mathrm{dst} (r e f 1)) \neq \operatorname{reg} (\mathrm{dst} (r e f 2)) & (2) \\ \operatorname{reg} (\operatorname{src} (r e f 1)) = \operatorname{reg} (\mathrm{dst} (r e f 1)) \lor \\ \quad \operatorname{reg} (\operatorname{src} (r e f 2)) = \operatorname{reg} (\mathrm{dst} (r e f 2)) & (3) \\ \operatorname{reg} (\mathrm{dst} (r e f 1)) \in \text { regions } (H _ {f r}) \lor \\ \quad \operatorname{reg} (\mathrm{dst} (r e f 2)) \in \text { regions } (H _ {f r}) & (4) \\ R S \vdash \mathrm{dst} (r e f 1) \preceq \operatorname{src} (r e f 1) \lor \\ R S \vdash \mathrm{dst} (r e f 2) \preceq \operatorname{src} (r e f 2) & (5) \end{array} \right.
$$

Fig. 10. The topology invariant 

must be the same reference ((1) holds). In particular, this means that there is at most one external reference into any closed region, implying that the region graph of closed regions forms a forest. 

The Topology Invariant and Fig. 1. Applying the topology invariant to all pairs of references in Fig. 1, assuming ?? was opened after $R ^ { \prime }$ , the reference $e \longrightarrow n$ is allowed to co-exist with any other alias of ?? since $R S \vdash n \preceq e \left( 5 \right)$ . The reference $m \to e$ is not allowed to co-exist with $m \to a$ since there would be two external references into the same region (1–5). However, $e \longrightarrow a$ can co-exist with $m \to a$ since the former stays within its region (3). The references $a \to i$ and $o \to i$ are allowed to co-exist because i is in a frozen region (4). Finally, $o \longrightarrow a$ is illegal both because it cannot co-exist with $m  a ,$ and since references in frozen regions cannot point to non-frozen regions. 

# 7.6 Reggio is Sound

We prove soundness of our system by proving variants of progress and preservation for the respective language. (The full proofs are available in the appendix.) 

Lemma 7.1. Command Language Progress A well-formed command configuration is done, has failed or can step: $\overline { { \Gamma } } \vdash \{ d e \} \implies d e = u s e \lor d e = F a i l e d \lor \exists E f f , d e ^ { \prime } . \{ d e \} \xrightarrow { E f f } \{ d e ^ { \prime } \}$ . 

Lemma 7.2. Command Language Preservation The command language preserves well-formedness and produces w $v e l l . f o r m e d ~ e f f e c t s : \overline { { \Gamma } } { \vdash } \{ d e \} \wedge \{ d e \} ~ \{ d e \} ~ \{ d e \} ~ \{ d e ^ { \prime } \} ~ \Longrightarrow ~ \exists \overline { { \Gamma } } ^ { \prime } . \overline { { \Gamma } } ^ { \prime } { \vdash } \{ d e ^ { \prime } \} ~ \wedge ~ \overline { { \Gamma } } { \vdash } ~ E f f ~ {  } ~ \overline { { \Gamma } } ^ { \prime }$ . 

The command language is more permissive than the region language, since it has no way of inspecting the state of the global configuration. For example, an enter can always both fail and succeed in the command language, whereas the region language always permits exactly one of the behaviours. This affects the formulation of progress: 

Lemma 7.3. Region Language Progress In a well-formed configuration where the command configuration can step, there is some effect which steps both configurations: ⊢ $\langle \{ d e \} r c f g \rangle \wedge \{ d e \} \xrightarrow { E f f }$ $\{ \acute { d e ^ { \prime } } \} \implies \exists E f f ^ { \prime } , \acute { d e ^ { \prime \prime } } , r c f g ^ { \prime } . \{ d e \} \xrightarrow { E f f ^ { \prime } } \{ d e ^ { \prime \prime } \} ~ \land ~ r c f g ~ \xrightarrow { E f f ^ { \prime } } ~ r c f g ^ { \prime }$ Eff ′−−−−→ rcfg′. 

Lemma 7.4. Region Language Preservation The region language preserves well-formedness for well-formed effects: $\overline { { { \Gamma } } } \vdash r c f g \land r c f g \xrightarrow { E f f } r c f g ^ { \prime } \land \overline { { { \Gamma } } } \vdash E f f \vdash | \overline { { { \Gamma } } } ^ { \prime } \implies \overline { { { \Gamma } } } ^ { \prime } \vdash r c f g ^ { \prime } \land$ . 

Note that Lemma 7.4 includes preservation of the topology invariant. Together, these lemmas prove the final soundness theorem: 

Theorem 7.5. Soundness A program never gets stuck and it preserves well-formedness: $\vdash \left. \left\{ d e \right\} \ r c f g \right. \ \Longrightarrow \ d e = u s e \lor d e = F a i l e d \lor \exists d e ^ { \prime } , \ r c f g ^ { \prime } . \left. \left\{ d e \right\} \ r c f g \right. \ \longrightarrow \left. \left\{ d e ^ { \prime } \right\} \ r c f g ^ { \prime } \right. \ \land$ $\vdash \langle \{ d e ^ { \prime } \} \ r c f g ^ { \prime } \rangle$ . 

# 8 REGGIO IN VERONA

While Reggio regions are a stand-alone language design component, they were developed specifically for the Verona programming language, from where the overarching goal (G1) stems. In this section, we describe Verona-specific aspects and revisit concurrency-related goals (G5) and (G6). 

# 8.1 Safe Concurrency

While regions and isolation can form the backbone of a “safe concurrency” story for a language, concurrency is an orthogonal aspect to our region design. Reggio regions can be integrated with different concurrency models. The necessary feature missing from this paper is a way to share regions across threads of control. 

Verona uses a concurrency model based on behaviours (tasks that do not join or have a return value) that operate on cowns, short for concurrent owners. A cown is a wrapper around an iso that permits regions to be indirectly shared across multiple threads of control, but importantly does not permit direct access to its contents. Cowns and iso’s are similar in that an explicit operation is needed to access their contents. In the case of iso’s, access is immediate and synchronous as exclusivity is already established. In the case of cowns, access is asynchronous and will only commence after exclusive access has been established dynamically. This check requires region isolation for soundness [Cheeseman et al. 2023] and as a result, any mutable reference accessible to a thread of control is safe to access synchronously (G6). 

For a complete introduction to Verona’s concurrency model, see work by Cheeseman et al. [2023]. 

# 8.2 Concurrent Memory Management

As memory management must only consider objects inside the active region (G3) when determining liveness $( c . f . , \ S 5 )$ , and regions are always exclusive to one thread, reference count manipulations do not need atomic instructions, tracing GC does not need barriers, and there is no need to momentarily stop all threads as in concurrent $\boldsymbol { \mathrm { G C } ^ { \prime } \mathrm { s } }$ [Click et al. 2005; Flood et al. 2016; Lidén and Karlsson 2018]. 

By extension, a thread in Verona is free to mediate between program work or memory management work without informing or synchronising with other threads. Thus, we achieve concurrent memory management (G5). 

# 8.3 Support for Different Memory Management Strategies

Verona currently supports three different memory management strategies for regions: arena allocation, reference counting, and tracing GC. 

How a region manages its memory is decided at use-site at creation time using a qualifier on the new keyword: new iso<Arena>, new iso<RC> and new iso<GC>. As liveness is a local property, different regions’ memory management does not interact, so we do not need to e.g., propagate this information further in the program. 

Selecting memory management at use-site is desirable since it lets a programmer implement a data structure or library without having to commit to decisions that could limit its future use. Such a design also allows straightforward support for libraries that consist of multiple nested regions whose memory management can be controlled when the library is instantiated by programmatic means, e.g., through a strategy pattern or equivalent. 

# 8.4 Memory Management of Immutable Objects

Note that the memory management offered by regions does not extend to immutable objects, at least not conceptually. One option for implementation is going the way of Erlang and let a region have a copy of each immutable object it references. This may facilitate fast reclamation, but increases memory pressure. Furthermore, it introduces a ?? (??) copying overhead for transferring immutable objects across region boundaries, in sharp contrast with (G4). 

In the Verona run-time, we permit immutable objects to be shared between regions. Thus, when a region is collected, we must detect its implications for liveness of immutable objects. Immutable objects must also consider roots across multiple threads. On the other hand, tracing immutable objects is easy and efficient as the structures are guaranteed not to change underfoot [Clebsch et al. 2017]. How Verona manages immutable objects is out of scope of this paper. 

# 8.5 Propagating Capabilities Through Self Typing

Verona is a class-based programming language. As an instance’s capability is determined at use-site, methods declare an explicit self-capability, e.g., self : mut to propagate the external view of the instance into the instance. A class may provide several different implementations of a method overloaded on the self-capability. 

A method can only be called on a receiver if its selfcapability matches the receiver’s static type, which means that the object’s treatment of itself internally will match the external view, both in terms of restrictions and abilities. For example, a method whose selfcapability is paused can only be called when the receiver’s region is paused. Notably, it is not permitted to call a paused method on a mut receiver because this would lead to aliasing between mut and paused (which would weaken paused from temporarily immutable to read-only). 

Methods that are polymorphic in their self capability can be used to avoid multiple near-identical versions of a single method like in Listing 5. For brevity, we refrain from discussing this further. 

```txt
class Cell {
    var value : I64 // mut Store[mut I64]
    def set_value(self:mut, v:I64) {
    // value : (mut ⊙ mut) Store[mut I64]
    value := v
    }
    def set_value(self:paused, v:I64) {
    // value : (paused ⊙ mut) Store[mut I64]
    value := v // does not typecheck!
    }
    def get_value(self:mut) = value
    def get_value(self:paused) = value
} 
```


Listing 5. Self typing in Cell.


# 9 RELATED WORK

We started out by describing related work leading up to Rust. We now extend this picture by going beyond Rust and also relating our work to garbage collection work before revisiting novelty. 

Beyond Rust. In addition to what we have already covered, there is continuing research into Rust to alleviate its restrictions, including incorporating a garbage collector [Coblenz et al. 2022], careful library design [Beingessner 2015], phantom types [Yanovski et al. 2021], or proving unsafe Rust code correct [Jung et al. 2019, 2017; Noble et al. 2022]. 

Recent research has focused on techniques for “post-Rust languages”, building on Rust’s use of ownership types, but supporting more flexible program topologies (and hopefully more efficient execution), typically by increasing the complexity of the type system. This remains an active research area: the tradeoffs between regions, ownership, types, capabilities, effects, topologies, restrictions etc are complex and multifaceted [Brachthäuser et al. 2022; Gordon 2020]. 

Pony [Clebsch et al. 2017; Franco et al. 2018] employs implicit regions, external uniqueness and ownership to offer high performance for actor programs by concurrent execution on multicore CPUs, while maintaining data-race and memory safety. Building on capabilities used to describe what programs can do with particular references [Boyland et al. 2001] Pony offers at least six “reference capabilities”: unique, thread-local, read-only, write-only, and identity-only, (globally) immutable, plus type modifiers for ephemeral (Hogg’s “free”) and aliased references. Reading or writing a field depends on the capabilities of both the reference to the object, and of the field within the object: there are 43 valid cases from 72 possible combinations of capabilities. 

Fernandez-Reyes et al. [2021] design Dala as a simplified alternative to Pony, based on three different kinds of objects—immutable, unique (aka isolated), and thread-local—rather than six different kinds of references. Dala programs are also data-race free, however this guarantee may be provided by a race detector at runtime, or by an optional/gradual type system. 

Milano et al.’s [2022] Gallifrey aims to be more flexible than Rust, by relying at least as much on MLKit style region inference as on ownership annotations. Rather than an explicit global ownership model, Gallifrey programmers have to annotate unique (aka isolated) object fields, and identify parameters that will be consumed by a method invocation or that should be in the same region as other parameters or the method result. A dynamic “if disconnected” predicate searches the program’s heap at runtime to determine of two references are mutually disjoint. 

Cogent [O’Connor et al. 2021] is a derivative of Haskell for systems programming. It adopts a Rust-like discipline, permitting either multiple read-only references to objects, or a single readwrite reference. Cogent uses annotations to support both a formally defined operational semantics, generation of executable C source code, and a proof certificate proving that the generated code accurately implements the semantics. 

Garbage Collection. Of the GC design goals, (G3) and (G5) can be met to some extent without region isolation. Here, Reggio’s contribution is trading additional work to manage regions (at development time) for reduced overheads of managing memory (at run-time) due to avoiding GC techniques like remembered sets, barrier synchronisation, and stop-the-world pauses. Region isolation guarantees that a region’s remembered set will always be empty. Thus, there is no cost associated with additional region partitioning due to tracking of inter-region references. 

With respect to (G3), generational GC’s (e.g., G1 [Detlefs et al. 2004]) and thread-local GC’s (e.g., [Domani et al. 2002]) support collecting only a portion of the heap (e.g., just the young generation or one particular thread), but the shape and size of this heap is beyond programmer control (e.g., all young or thread-local objects will take part of GC, not just particular data structures). Furthermore, in the absence of (something like) region isolation, inter-region aliases must be tracked dynamically to be able to correctly compute liveness. Actor GC’s that rely on actor isolation using types (e.g., Pony [Clebsch et al. 2017; Franco et al. 2018]) or copying (e.g., Erlang [Armstrong 2007]) are close as they allow individual actor-local heaps to be collected. This is similar to Reggio’s regions, but Reggio supports additional partitioning of the heap without an imposed asynchronous indirection. 

With respect to (G2), while it may be possible to run different GC’s in different generations (or threads, actors, etc.), GC’s typically use the same algorithm for the entire heap, with minor tweaks (e.g., to account for different object characteristics due to age) as do actor GC’s. HRTGC is a real-time GC for mixed-criticality work-loads [Pizlo et al. 2007] that hierarchically decomposes the heap into regions that each run a different tracing GC, tuned differently and with different collection frequency. HRTGC permits inter-region references and tracks them dynamically. In the actor world, Isolde [Yang and Wrigstad 2017] permits actors implemented using type-enforced actor isolation [Castegren and Tobias Wrigstad 2016; Castegren and Wrigstad 2017] to manage their memory concurrent with their execution, using a reference-counting based scheme. 

With respect to (G5), garbage collectors like C4 [Tene et al. 2011], Shenandoah [Flood et al. 2016] and ZGC [Lidén and Karlsson 2018] provide concurrent collection with brief stop-the-world pauses to coordinate phase changes, with pause times invariant of heap sizes. Their heaps have unrestricted references, and instead rely on dynamic checks in read and write barriers. The aforementioned actor GC’s [Armstrong 2007; Clebsch et al. 2017; Franco et al. 2018] support “fully concurrent” 

collection: an actor can choose to collect its local garbage without synchronising with any other concurrent activity. Reggio’s regions additionally allow us to statically detect when an entire region is invalidated, without the need for a specific actor collector to eventually detect the floating garbage (e.g., [Clebsch and Drossopoulou 2013]). Explicitly killing an actor to instantly free its heap is a common programming pattern in Erlang where it is made possible by copying objects on transfer, i.e., giving up (G4). 

# 10 DISCUSSION

First, we place Verona’s design concepts into the context of all related work. Almost any ownership system paired with external uniqueness will support region isolation and dynamic reconfiguration. Verona’s key contribution here is a negative contribution, but important nonetheless. Almost all other systems provide one or more top, global, or shared heap region, and in various ways permit references from inner/encapsulated/shorter-lived regions back to outer/enclosing/longer-lived regions. (Generational garbage collection works on a very similar principle [Jones et al. 2016]). Verona does not, and this enhanced decoupling of regions is critical to achieving many of our goals, especially about concurrency, and independent GC. 

Verona’s dynamic mutability and relaxed isolation is novel and differs from other ownership and region systems. Inasmuch as region systems like the MLKit are based on inference, and are sound for all legal programs, questions of mutability and isolation don’t apply—if the program is type-correct, the inference system can always place objects into regions such that no region errors will arise at runtime. Programming with more explicit regions, or with ownership and capability annotations, either lack polymorphism (e.g., Dala) or require complex resolution or viewpoint adaptation rules, or asynchronous indirections (e.g., Pony). In a way, Verona’s region system trades precision for simplicity. It cannot construct data structures that consist of several morally overlapping regions, as is possible in e.g., C++ or Rust. We believe all programs written in e.g., C++ or Rust pay the price for that precision—even though most programs do not need it. 

Verona’s “single window of mutability” is probably its most novel concept. In pretty much every language, from FORTRAN to LISP to ML to Haskell to C++ to Pony to Gallifrey, if some code can finagle a mutable reference to an object, the program can always update the object through that reference. In Rust for example, programs can collect up “mutable borrows” (&mut) of any number of objects, pass them around as method arguments to anywhere in the program, and then mutate all the borrowed objects. Rust’s “interior mutability” (aka C++’s const-cast) just increases the scope for potential mutation. This kind of indiscriminate mutation is exactly what the “single window of mutability” in Verona prevents. Once a program departs—even temporarily—from the scope of an opened region, (e.g., by opening some other closed region) the program can no longer modify anything in that first opened region, no matter what kind of objects are in the region, nor what kind of capability or reference the program has to those objects. Perhaps a single window of mutability will prove too restrictive in practice, which may be why no other system has yet adopted it. Verona demonstrates that it is possible to build a system with a single mutability window as a core design concept; exploring that concept further must necessarily be further work. 

The single window of mutability is key to simplicity, both with respect to the type system that enforces region isolation and our invariants for memory management. We need only distinguish objects in the active region, from objects in suspended regions and objects in closed regions. As suspended regions are immutable and closed regions inaccessible, we do not need to distinguish objects belonging to different regions as nothing can be done to them that affects or is affected by their region membership. By having only a single mutable region at a time, non-local operations cannot effect object liveness in the active region, or the liveness of an entire active region. This permits optimisations at the implementation level and simplifies the task of the programmer wishing to reason about—and control—memory management performance. 

In contrast to other works which use types to enforce (or impose) a structure on the heap, we let the path of the program through the heap dictate the permissible pointer structure—not the other way around. For example, Verona allows a single point in the program to access the contents of two mutually isolated regions ?? and ??, simply by virtue of opening them in a nested fashion. The key insight is the decoupling of accessing from mutating, implemented through the movable window of mutability. Rather than alternating between accessing ?? and ??, we can gain access to first ?? and then ?? without giving up access to ??, just the rights to mutate it. Opening ?? after ?? allows references to objects in ?? from ?? to be created freely, but these references may only persist as long as ?? remains open. When ?? is closed, the references to ?? are invalidated. This retains the flexibility of navigating heap structures in any order, e.g., we could close ?? then ?? and open them immediately in the opposite order, allowing pointers from ?? to ??. It also ensures that an object is either mutable or immutable at any moment in time. 

# 11 CONCLUSION

We have presented Reggio, a region system enforced by reference capabilities that partitions a program’s heap into a forest of isolated regions. Memory in different regions can be managed differently (G2), incrementally (G3) and concurrently (G5). The single external reference to each region plus their full encapsulation enable cheap ownership transfer (G4) and guarantees freedom from data races (G6). The ability to temporarily trade mutability for access on the region stack allows any region to (temporarily) reference any other region, and also allows “external code” to operate inside a region, which is crucial for libraries and reuse—not all uses of a region can be predicted and supplied in its interface. In combination with the region isolation and the single window of mutability this allows the formulation of topological invariants which are useful for a programmer to control and reason about object liveness and implications of memory management (G1) and can be leveraged for efficient implementation of memory management. Memory management costs are only incurred by the active region (one per thread), and data accesses within that region—whether reading, writing, tracing, or reference counting—never need atomic operations to coordinate with other threads. 

# ACKNOWLEDGMENTS

This work was partially supported by a grant from the Swedish Research Council (2020-05346), and partially by the Royal Society of New Zealand Te Aparangi Marsden Fund Te P ¯ utea Rangahau ¯ a Marsden grants CRP1801 and CRP2101, and Agoric. We thank the anonymous reviewers at OOPSLA’23 for their input that greatly improved the presentation of this paper. 

# REFERENCES



Parastoo Abtahi and Griffin Dietz. 2020. Learning Rust: How Experienced Programmers Leverage Resources to Learn a New Programming Language. In CHI Extended Abstracts. 1–8. 





Jonathan Aldrich and Craig Chambers. 2004. Ownership domains: Separating aliasing policy from mechanism. In ECOOP, Vol. 4. Springer, 1–25. 





J. Armstrong. 2007. A History of Erlang. In HOPL III. https://doi.org/10.1145/1238844.1238850 





Ellen Arvidsson, Elias Castegren, Sylvan Clebsch, Sophia Drossopoulou, James Noble, Matthew J. Parkinson, and Tobias Wrigstad. 2023. Reference Capabilities for Flexible Memory Management. Proceedings of the ACM on Programming Languages 7, OOPSLA2 (10 2023). https://doi.org/10.1145/3622846 





Henry G. Baker. 1990. Unify and Conquer (Garbage, Updating, Aliasing,. . . ) in Functional Languages. In LISP and Functional Programming. 218–226. 





Aria Beingessner. 2015. You can’t spell Trust without Rust. Master’s thesis. Computer Science, Carleton University. 





Aria Beingessner. 2019. Learn Rust With Entirely Too Many Linked Lists. https://rust-unofficial.github.io/- too-many-lists. Accessed 2023-04-14. 





Eli Bendersky. 2021. Rust data structures with circular references. eli.thegreenplace.net/2021/rust-data-structures-with-circular-references/. 





David Blaser. 2019. Simple Explanation of Complex Lifetime Errors in Rust. (2019). Bachelor Thesis, ETH Zürich. 





Robert Bocchino. 2011. Deterministic Parallel Java. In Encyclopedia of Parallel Computing. 566–573. https://doi.org/10.1007/ 978-0-387-09766-4_119 





Gregory Bollella, Tim Canham, Vanessa Carson, Virgil Champlin, Daniel L. Dvorak, Brian Giovannoni, Mark B. Indictor, Kenny Meyer, Alex Murray, and Kirk Reinholtz. 2003. Programming with non-heap memory in the real time specification for Java. In OOPSLA Companion. 361–369. 





Chandrasekhar Boyapati and Martin Rinard. 2001. A Parameterized Type System for Race-Free Java Programs. In Proceedings of the 16th ACM SIGPLAN Conference on Object-Oriented Programming, Systems, Languages, and Applications (OOPSLA ’01). Association for Computing Machinery, New York, NY, USA, 56–69. https://doi.org/10.1145/504282.504287 





Chandrasekhar Boyapati, Alexandru Salcianu, William Beebee, and Martin Rinard. 2003. Ownership Types for Safe Region-Based Memory Management in Real-Time Java. In Proceedings of the ACM SIGPLAN 2003 Conference on Programming Language Design and Implementation (San Diego, California, USA) (PLDI ’03). Association for Computing Machinery, New York, NY, USA, 324–337. https://doi.org/10.1145/781131.781168 





John Boyland. 2001. Alias burying: Unique variables without destructive reads. Software: Practice and Experience 31, 6 (2001), 533–553. https://doi.org/10.1002/spe.370 Publisher: Wiley. 





John Boyland. 2013. Fractional Permissions. In Aliasing in Object-Oriented Programming, Dave Clarke, James Noble, and Tobias Wrigstad (Eds.). Springer-Verlag, Berlin, Heidelberg, 270–288. https://doi.org/10.1007/978-3-642-36946-9_10 





John Boyland, James Noble, and William Retert. 2001. Capabilities for Sharing. In Proceedings of the 15th European Conference on Object-Oriented Programming ECOOP, Budapest, Hungary, June 18-22, 2001. Springer Berlin Heidelberg, 2–27. https://doi.org/10.1007/3-540-45337-7_2 





Jonathan Immanuel Brachthäuser, Philipp Schuster, Edward Lee, and Aleksander Boruch-Gruszecki. 2022. Effects, Capabilities, and Boxes: From Scope-Based Reasoning to Type-Based Reasoning and Back. Proc. ACM Program. Lang., Article 76 (apr 2022), 30 pages. https://doi.org/10.1145/3527320 





Nicholas Cameron. 2015. What’s the “best” way to implement a doubly-linked list in Rust? http://featherweightmusings.blogspot.com/2015/04/graphs-in-rust.html. Accessed 2023-04-14. 





Elias Castegren and Tobias Wrigstad. 2016. Reference Capabilities for Concurrency Control. In 30th European Conference on Object-Oriented Programming, ECOOP 2016, July 18-22, 2016, Rome, Italy. 5:1–5:26. https://doi.org/10.4230/LIPIcs.ECOOP. 2016.5 





Elias Castegren and Tobias Wrigstad. 2017. Relaxed Linear References for Lock-free Data Structures. In Proceedings of the 31st European Conference on Object-Oriented Programming ECOOP (Leibniz International Proceedings in Informatics (LIPIcs), Vol. 74). Schloss Dagstuhl–Leibniz-Zentrum fuer Informatik, 6:1–6:32. https://doi.org/10.4230/LIPIcs.ECOOP.2017.6 ISSN: 1868-8969. 





Luke Cheeseman, Matthew J. Parkinson, Sylvan Clebsch, Marios Kogias, Sophia Drossopoulou, David Chisnall, Tobias Wrigstad, and Paul Liétar. 2023. When Concurrency Matters. Proceedings of the ACM on Programming Languages 7, OOPSLA2 (10 2023). https://doi.org/10.1145/3622852 





David Clarke. 2001. Object Ownership and Containment. Ph.D. Dissertation. University of New South Wales. 





Dave Clarke and Sophia Drossopoulou. 2002. Ownership, Encapsulation and the Disjointness of Type and Effect. In Proceedings of the 17th ACM SIGPLAN Conference on Object-Oriented Programming, Systems, Languages, and Applications (Seattle, Washington, USA) (OOPSLA ’02). Association for Computing Machinery, New York, NY, USA, 292–310. https: //doi.org/10.1145/582419.582447 





Dave Clarke and Tobias Wrigstad. 2003. External Uniqueness Is Unique Enough. In ECOOP 2003 – Object-Oriented Programming, Luca Cardelli (Ed.). Springer Berlin Heidelberg, Berlin, Heidelberg, 176–200. 





David Clarke, Tobias Wrigstad, and James Noble. 2013. Aliasing in Object-oriented Programming: Types, Analysis and Verification. Vol. 7850. Springer. 





Dave Clarke, Tobias Wrigstad, Johan Östlund, and Einar Broch Johnsen. 2008. Minimal Ownership for Active Objects. In Programming Languages and Systems, G. Ramalingam (Ed.). Springer Berlin Heidelberg, Berlin, Heidelberg, 139–154. 





David G. Clarke, John M. Potter, and James Noble. 1998. Ownership Types for Flexible Alias Protection. In Proceedings of the 13th ACM SIGPLAN Conference on Object-Oriented Programming, Systems, Languages, and Applications (Vancouver, British Columbia, Canada) (OOPSLA ’98). Association for Computing Machinery, New York, NY, USA, 48–64. https: //doi.org/10.1145/286936.286947 





Sylvan Clebsch and Sophia Drossopoulou. 2013. Fully Concurrent Garbage Collection of Actors on Many-core Machines. In Proceedings of the 2013 ACM SIGPLAN International Conference on Object Oriented Programming Systems Languages & Applications, OOPSLA 2013, part of SPLASH 2013, Indianapolis, IN, USA, October 26-31, 2013. 553–570. https://doi.org/10. 





1145/2509136.2509557 





Sylvan Clebsch, Juliana Franco, Sophia Drossopoulou, Albert Mingkun Yang, Tobias Wrigstad, and Jan Vitek. 2017. Orca: GC and Type System Co-design for Actor Languages. PACMPL 1, OOPSLA (2017), 72:1–72:28. https://doi.org/10.1145/3133896 





Cliff Click, Gil Tene, and Michael Wolf. 2005. The Pauseless GC Algorithm. In Proceedings of the 1st ACM/USENIX International Conference on Virtual Execution Environments (VEE ’05). ACM, New York, NY, USA, 46–56. https://doi.org/10.1145/ 1064979.1064988 event-place: Chicago, IL, USA. 





Michael Coblenz, Michelle L. Mazurek, and Michael Hicks. 2022. Garbage Collection Makes Rust Easier to Use: A Randomized Controlled Trial of the Bronze Garbage Collector. In Proceedings of the 44th International Conference on Software Engineering (Pittsburgh, Pennsylvania) (ICSE ’22). Association for Computing Machinery, New York, NY, USA, 1021–1032. https: //doi.org/10.1145/3510003.3510107 





Russell Cohen. 2018. Why Writing a Linked List in (safe) Rust is So Damned Hard. https://rcoh.me/posts/- rust-linked-list-basically-impossible/. Accessed 2023-04-14. 





CWE 2022. 2021 CWE Top 25 Most Dangerous Software Weaknesses. https://cwe.mitre.org/top25/archive/2021/2021_cwe_ top25.html. 





David Detlefs, Christine Flood, Steve Heller, and Tony Printezis. 2004. Garbage-First Garbage Collection. In Proceedings of the 4th International Symposium on Memory Management (Vancouver, BC, Canada) (ISMM ’04). Association for Computing Machinery, New York, NY, USA, 37–48. https://doi.org/10.1145/1029873.1029879 





Werner Dietl, Sophia Drossopoulou, and Peter Müller. 2007. Generic Universe Types. In ECOOP 2007 - Object-Oriented Programming, 21st European Conference, Berlin, Germany, July 30 - August 3, 2007, Proceedings. 28–53. https://doi.org/10. 1007/978-3-540-73589-2_3 





Tamar Domani, Gal Goldshtein, Elliot K. Kolodner, Ethan Lewis, Erez Petrank, and Dafna Sheinwald. 2002. Thread-Local Heaps for Java. In Proceedings of the 3rd International Symposium on Memory Management (Berlin, Germany) (ISMM ’02). Association for Computing Machinery, New York, NY, USA, 76–87. https://doi.org/10.1145/512429.512439 





Martin Elsman and Niels Hallenberg. 2021. Integrating region memory management and tag-free generational garbage collection. J. Funct. Program. 31 (2021), e4. https://doi.org/10.1017/S0956796821000010 





Kiko Fernandez-Reyes, Isaac Oscar Gariano, James Noble, Erin Greenwood-Thessman, Michael Homer, and Tobias Wrigstad. 2021. Dala: A Simple Capability-Based Dynamic Language Design For Data Race-Freedom. In Proceedings of the 2021 ACM SIGPLAN International Symposium on New Ideas, New Paradigms, and Reflections on Programming and Software (Chicago, IL, USA) (Onward! 2021). Association for Computing Machinery, New York, NY, USA, 1–17. https://doi.org/10. 1145/3486607.3486747 





Cormac Flanagan and Stephen N. Freund. 2000. Type-Based Race Detection for Java. In Proceedings of the ACM SIGPLAN 2000 Conference on Programming Language Design and Implementation (Vancouver, British Columbia, Canada) (PLDI ’00). Association for Computing Machinery, New York, NY, USA, 219–232. https://doi.org/10.1145/349299.349328 





Christine H. Flood, Roman Kennke, Andrew Dinn, Andrew Haley, and Roland Westrelin. 2016. Shenandoah: An open-source concurrent compacting garbage collector for OpenJDK. In Proceedings of the 13th International Conference on Principles and Practices of Programming on the Java Platform: Virtual Machines, Languages, and Tools, Lugano, Switzerland, August 29 - September 2, 2016. 13:1–13:9. https://doi.org/10.1145/2972206.2972210 





Matthew Fluet and Greg Morrisett. 2006. Monadic regions. J. Funct. Program. 16, 4-5 (2006), 485–545. 





Matthew Fluet, Greg Morrisett, and Amal J. Ahmed. 2006. Linear Regions Are All You Need. In ESOP, Vol. 3924. 7–21. 





Matthew Francis-Landau, Bing Xue, Jason Eisner, and Vivek Sarkar. 2016. Fine-grained parallelism in probabilistic parsing with Habanero Java. In IA3 ’16: Proceedings of the Sixth Workshop on Irregular Applications: Architectures and Algorithms. IEEE Press, Piscataway, NJ, USA, 78–81. event-place: Salt Lake City, Utah. 





Juliana Franco, Sylvan Clebsch, Sophia Drossopoulou, Jan Vitek, and Tobias Wrigstad. 2018. Correctness of a fully concurrent Garbage Collector for Actor Languages. In European Symposium on Programming (ESOP), Vol. 10801. 





David Gay and Alex Aiken. 1998. Memory Management with Explicit Regions. SIGPLAN Not. 33, 5 (may 1998), 313–323. 





David Gay and Alex Aiken. 2001. Language Support for Regions. SIGPLAN Not. 36, 5 (may 2001), 70–80. 





Laure Gonnord, Ludovic Henrio, Lionel Morel, and Gabriel Radanne. 2023. A Survey on Parallelism and Determinism. ACM Comput. Surv. 55, 10, Article 210 (feb 2023), 28 pages. https://doi.org/10.1145/3564529 





Colin S. Gordon. 2020. Designing with Static Capabilities and Effects: Use, Mention, and Invariants (Pearl). In ECOOP, Robert Hirschfeld and Tobias Pape (Eds.). 10:1–10:25. 





Colin S. Gordon, Matthew J. Parkinson, Jared Parsons, Aleks Bromfield, and Joe Duffy. 2012. Uniqueness and reference immutability for safe parallelism. In Proceedings of the 27th Annual ACM SIGPLAN Conference on Object-Oriented Programming, Systems, Languages, and Applications, OOPSLA 2012, part of SPLASH 2012, Tucson, AZ, USA, October 21-25, 2012. 21–40. https://doi.org/10.1145/2384616.2384619 





Dan Grossman, Greg Morrisett, Trevor Jim, Michael Hicks, Yanling Wang, and James Cheney. 2002. Region-based memory management in Cyclone. ACM Sigplan Notices 37, 5 (2002), 282–293. Publisher: ACM. 





Olivier Gruber and Fabienne Boyer. 2013. Ownership-Based Isolation for Concurrent Actors on Multi-core Machines. In ECOOP 2013 – Object-Oriented Programming, Giuseppe Castagna (Ed.). Springer Berlin Heidelberg, Berlin, Heidelberg, 281–301. 





Philipp Haller and Alexander Loiko. 2016. LaCasa: lightweight affinity and object capabilities in Scala. In Proceedings of the 2016 ACM SIGPLAN International Conference on Object-Oriented Programming, Systems, Languages, and Applications, OOPSLA 2016, part of SPLASH 2016, Amsterdam, The Netherlands, October 30 - November 4, 2016. 272–291. https: //doi.org/10.1145/2983990.2984042 





Philipp Haller and Martin Odersky. 2010. Capabilities for Uniqueness and Borrowing. In Proceedings of 24th European Conference on Object-Oriented Programming, ECOOP, Maribor, Slovenia, June 21-25. https://doi.org/10.1007/978-3-642- 14107-2_17 





Douglas E. Harms and Bruce W. Weide. 1991. Copying and Swapping: Influences on the Design of Reusable Software Components. IEEE Trans. Softw. Eng. 17, 5 (May 1991), 424–435. https://doi.org/10.1109/32.90445 Publisher: IEEE Press. 





Michael W. Hicks, J. Gregory Morrisett, Dan Grossman, and Trevor Jim. 2004. Experience with safe manual memorymanagement in Cyclone. In ISMM. 73–84. 





John Hogg. 1991. Islands: Aliasing Protection in Object-Oriented Languages. In Conference proceedings on Object-oriented programming systems, languages, and applications - OOPSLA ’91. ACM Press, 271–285. https://doi.org/10.1145/117954. 117975 





J. Hogg, D. Lea, A. Wills, D. de Champeaux, and R. Holt. 1992. The Geneva Convention on the Treatment of Object Aliasing. OOPS Messenger 3, 2 (April 1992). 





Vivian Hu. 2020. Rust Breaks into TIOBE Top 20 Most Popular Programming Languages. (June 2020). InfoQ. 





Daniel H. Ingalls. 1981. Design Principles Behind Smalltalk. BYTE 6, 8 (August 1981), 286–298. 





Richard Jones, Antony Hosking, and Eliot Moss. 2016. The garbage collection handbook: the art of automatic memory management. CRC Press. 





Ralf Jung, Hoang-Hai Dang, Jeehoon Kang, and Derek Dreyer. 2019. Stacked Borrows: An Aliasing Model for Rust. Proc. ACM Program. Lang. 4, POPL, Article 41, 32 pages. https://doi.org/10.1145/3371109 





Ralf Jung, Jacques-Henri Jourdan, Robbert Krebbers, and Derek Dreyer. 2017. RustBelt: Securing the Foundations of the Rust Programming Language. PACMPL 2, POPL, Article 66 (Jan. 2017), 66:1–66:34 pages. 





Ralf Jung, Jacques-Henri Jourdan, Robbert Krebbers, and Derek Dreyer. 2020. Safe Systems Programming in Rust: The Promise and the Challenge. Communications of the ACM (2020). 





Steve Klabnik and Carol Nichols. 2019. The Rust Programming Language (Covers Rust 2018). No Starch Press. 





Paul Krill. 2021. Microsoft forms Rust language team. (Feb. 2021). InfoWorld. 





Butler Lampson, Jim Horning, Ralph London, Jim Mitchell, and Gerry Popek. 1977. Report on the Programming Language Euclid. ACM Sigplan Notices 12, 3 (March 1977), 18–79. 





Doug Lea. 1998. Concurrent Programming in Java (2nd ed.). Addison-Wesley. 





Ole Lehrmann Madsen, Birger Møller-Pedersen, and Kristen Nygaard. 1993. Object-Oriented Programming in the BETA Programming Language. Addison-Wesley. 





Paley Li, Nicholas Cameron, and James Noble. 2012. Sheep Cloning with Ownership Types. In Foundations of Object-Oriented Programming Languages (FOOL). ACM. 





Per Lidén and Stefan Karlsson. 2018. The Z Garbage Collector—Low Latency GC for OpenJDK. http://cr.openjdk.java.net/ pliden/slides/ZGC-Jfokus-2018.pdf 





Karl J. Lieberherr and Ian Holland. 1989. Assuring Good Style for Object-Oriented Programs. IEEE Software September (1989), 38–48. 





Luis Mastrangelo, Luca Ponzanelli, Andrea Mocci, Michele Lanza, Matthias Hauswirth, and Nathaniel Nystrom. 2015. Use at Your Own Risk: The Java Unsafe API in the Wild. In Proceedings of the 2015 ACM SIGPLAN International Conference on Object-Oriented Programming, Systems, Languages, and Applications (Pittsburgh, PA, USA) (OOPSLA 2015). Association for Computing Machinery, New York, NY, USA, 695–710. https://doi.org/10.1145/2814270.2814313 





Mae Milano, Joshua Turcotti, and Andrew C. Myers. 2022. A flexible type system for fearless concurrency. In PLDI ’22: 43rd ACM SIGPLAN International Conference on Programming Language Design and Implementation, San Diego, CA, USA, June 13 - 17, 2022, Ranjit Jhala and Isil Dillig (Eds.). ACM, 458–473. https://doi.org/10.1145/3519939.3523443 





Peter Müller and Arnd Poetzsch-Heffter. 1999. Universes: A type system for controlling representation exposure. In Programming Languages and Fundamentals of Programming, Vol. 263. Technical Report 263, Fernuniversität Hagen. http://www. informatik. fernuni-hagen. de/pi5/publications. html. 





ndrewxie. 2019. What’s the “best” way to implement a doubly-linked list in Rust? https://users.rust-lang.org/t/- whats-the-best-way-to-implement-a-doubly-linked-list-in-rust/27899/7. Accessed 2023-04-14. 





James Noble, Julian Mackay, and Tobias Wrigstad. 2022. Rusty Links in Local Chains. In FTfJP. 





James Noble, Jan Vitek, and John Potter. 1998. Flexible Alias Protection. In ECOOP’98 — Object-Oriented Programming, Eric Jul (Ed.). Springer Berlin Heidelberg, Berlin, Heidelberg, 158–185. 





James Noble and Charles Weir. 2000. Small Memory Software: Patterns for Systems with Limited Memory. Addison-Wesley. 





Liam O’Connor, Zilin Chen, Christine Rizkallah, Vincent Jackson, Sidney Amani, Gerwin Klein, Toby Murray, Thomas Sewell, and Gabriele Keller. 2021. Cogent: uniqueness types and certifying compilation. Journal of Functional Programming 31 (2021), e25. 





Filip Pizlo, Antony L. Hosking, and Jan Vitek. 2007. Hierarchical Real-Time Garbage Collection. In Proceedings of the 2007 ACM SIGPLAN/SIGBED Conference on Languages, Compilers, and Tools for Embedded Systems (San Diego, California, USA) (LCTES ’07). Association for Computing Machinery, New York, NY, USA, 123–133. https://doi.org/10.1145/1254766. 1254784 





J. Potter, J. Noble, and D. Clarke. 1998. The Ins and Outs of Objects. In ASWEC. 





Boqin Qin, Yilun Chen, Zeming Yu, Linhai Song, and Yiying Zhang. 2020. Understanding memory and thread safety practices and issues in real-world Rust programs. In PLDI. 763–779. 





Ryan James Spencer. 2020. Four Ways To Avoid The Wrath Of The Borrow Checker. (2020). justanotherdot.com. 





S. Srinivasan and A. Mycroft. 2008. Kilim: Isolation-Typed Actors for Java. In ECOOP. https://doi.org/10.1007/978-3-540- 70592-5_6 





Gil Tene, Balaji Iyengar, and Michael Wolf. 2011. C4: The Continuously Concurrent Compacting Collector. In Proceedings of the International Symposium on Memory Management (ISMM ’11). ACM, New York, NY, USA, 79–88. https://doi.org/10. 1145/1993478.1993491 event-place: San Jose, California, USA. 





Tiobe 2022. TIOBE Index for June 2022. https://www.tiobe.com/tiobe-index/. 





Mads Tofte, Lars Birkedal, Martin Elsman, and Niels Hallenberg. 2004. A Retrospective on Region-Based Memory Management. Higher Order Symbolic Computing 17, 3 (2004), 245–265. https://doi.org/10.1023/B:LISP.0000029446.78563.a4 





Mads Tofte, Lars Birkedal, Martin Elsman, Niels Hallenberg, Tommy Højfeld Olesen, and Peter Sestoft. 2021. Programming with Regions in the MLKit (Revised for Version 4.6.0). Technical Report. Department of Computer Science, University of Copenhagen, Denmark. 





Mads Tofte and Jean-Pierre Talpin. 1997. Region-based Memory Management. Inf. Comput. 132, 2 (1997), 109–176. https://doi.org/10.1006/inco.1996.2613 





Aaron Turon. 2015. Fearless Concurrency with Rust. https://blog.rust-lang.org/2015/04/10/Fearless-Concurrency.html 





Mark Utting. 1995. Reasoning about aliasing. In Fourth Australasian Refinement Workshop. 





Philip Wadler. 1990. Linear types can change the world. In IFIP TC, Vol. 2. Citeseer, 347–359. 





Tobias Wrigstad. 2006. Ownership-Based Alias Management. Ph.D. Dissertation. Royal Institute of Technology, Stockholm. 





Albert Mingkun Yang and Tobias Wrigstad. 2017. Type-Assisted Automatic Garbage Collection for Lock-Free Data Structures. In Proceedings of the 2017 ACM SIGPLAN International Symposium on Memory Management (Barcelona, Spain) (ISMM 2017). Association for Computing Machinery, New York, NY, USA, 14–24. https://doi.org/10.1145/3092255.3092274 





Joshua Yanovski, Hoang-Hai Dang, Ralf Jung, and Derek Dreyer. 2021. GhostCell: separating permissions from data in Rust. In ICFP. 



# A INTRODUCTION TO APPENDIX

This appendix contains the static and dynamic semantic of Reggio, and the full proof of soundness. The dynamic semantics is defined as two systems executing in tandem: the region semantics and the command semantics. The region semantics is the most important dynamic semantics in terms of understanding the system. It is responsible for entering and exiting regions, handling allocations and variable bindings. (We do not model deallocation.) The command semantics drives execution by emitting effects which are then performed by the region semantics. 

Any differences between the main paper and the appendix is purely for presentational reasons. 

# B TANDEM SEMANTICS

$$
c f g \quad : := \quad \langle \{d e \} r c f g \rangle \quad \text { Tandem   configuration }
$$

A Verona configuration is a tuple of a command configuration and a region configuration. It steps if the two configurations can both step with the same effect, and is well-formed if both configurations are well-formed with the same Γ. 

$$
\boxed {c f g \to c f g ^ {\prime}}
$$

(Tandem Semantics) 


tandem-step


<table><tr><td>$ de \xrightarrow{Eff} de&#x27; $</td><td>$ rcfg \xrightarrow{Eff} rcfg&#x27; $</td></tr><tr><td colspan="2">$ \langle\{de\}rcfg\rangle \to \langle\{de&#x27;\}rcfg&#x27;\rangle $</td></tr></table>

$$
\boxed {\vdash c f g}
$$

(Tandem Semantics) 


tandem-wf


<table><tr><td><eq>\overline{\Gamma} \vdash \{ de \}</eq></td><td><eq>\overline{\Gamma} \vdash rcfg</eq></td></tr><tr><td colspan="2"><eq>\vdash \langle \{ de \} rcfg \rangle</eq></td></tr></table>

# C REGION SEMANTICS

$$
\begin{array}{l} r \in R e g \quad \text {   Region   ids   } \\ \iota \in O i d \quad \text { Object   ids } \\ x, y, z, w, f, g \in V a r \quad \text { Field   and   variable   names } \\ C \in C l s \quad \text { Class   names } \\ \end{array}
$$

$$
\begin{array}{l} C L \quad : := \quad C \mid \operatorname{Cell} [ t ] \quad \text {   Static   types   } \\ \# C L \quad : := \quad \# C \mid \# \text { Cell } \quad \text { Dynamic   type   tags } \\ k \quad : := \quad \text { iso } \mid \text { var } \mid \text { mut } \mid \text { tmp } \mid \text { paused } \mid \text { imm } \quad \text { Capabilities } \\ t \quad : := \quad k C L \mid t \mid t \quad \text { Types } \\ v \quad : := \quad (k, \iota) \quad \text { Values   (references   tagged   with   capabilities) } \\ o \quad : := \quad (\# C L, F) \quad \text { Objects   (class   tag   and   field) } \\ F \quad : := \quad x \mapsto v, F \mid x \mapsto \mathbf {u n d e f}, F \mid \varepsilon \quad \text { Field   and   variable   bindings } \\ R \quad : := \quad (r, S) \quad \text {Region (region id and sub - heap)} \\ S \quad : := \quad \iota \mapsto o, S \mid \varepsilon \quad \text { Store } \\ H \quad : := \quad H * H \mid R \mid \varepsilon \quad \text {   Heap   (collection   of   Regions)   } \\ R F \quad : := \quad (r, S, F) \quad \text {   Region   frame   } \\ R S \quad : := \quad R F:: R S \mid \varepsilon \quad \text {   Region   stack   } \\ r c f g \quad : := \quad \langle R S; H; H; H \rangle \quad \text { Configuration } \\ \end{array}
$$

The heap ?? is a collection of disjoint regions ??, each containing a region identifiers ?? and a store ?? mapping object identifiers ?? to objects ??. An object consists of the object’s class tag (without the type parameter of Cell) and a mapping ?? from field names ?? to values ?? (consisting of a capability ?? and an object identifier ??). The region configuration rcfg is a region stack RS and three heaps ?? , for the open, closed and frozen regions respectively. The region stack is a stack of frames RF holding a region identifier, a (local) store ?? and a mapping ?? from variables to values. Note that for simplicity we reuse ?? for storing field mappings and variable mappings, meaning fields and variables are in the same syntactic category Var. 

We assume the existence of a table of classes and their fields such that ftypes $( C L ) = f _ { 1 } : t _ { 1 } , . . . , f _ { n }$ : $t _ { n }$ and $\mathbf { f t y p e } ( C L , f _ { i } ) = t _ { i } .$ , meaning that the class ???? has a field ???? with type $t _ { i } .$ . We also assume that no field has capability var. We use fields(????) to get just the names of the fields. We further assume that we can always create fresh identifiers that do not appear elsewhere in a configuration. 

# C.1 Dynamic Semantics

In order to avoid duplicating rules depending on whether reads are destructive or not, we define the following helper function: 

$$
\begin{array}{l} \mathbf {g e t} (F [ x \mapsto v ], \quad \mathbf {d r o p} x) = (v, F [ x \mapsto \mathbf {u n d e f} ]) \\ \mathbf {g e t} (F [ x \mapsto (k, \iota) ], \quad x) = ((k, \iota), F [ x \mapsto (k, \iota) ]) \quad \text { if } k \neq \operatorname{var} \wedge k \neq \text { iso } \\ \mathbf {g e t} (F, \quad u s e) = \downarrow \quad \text { otherwise } \\ \end{array}
$$

where ↓ denotes the undefined value. 

$$
\boxed {r c f g \xrightarrow {E f f} r c f g ^ {\prime}}
$$

(Region semantics) 

region-load 

$$
x \text {   fresh   } \quad F (y) = (k, \iota)
$$

$$
\operatorname{cfg} _ {-} \operatorname{load} ((r, S, F):: R S, H _ {o p} * H _ {f r}, \iota) = o [ f \mapsto v ]
$$

$$
F ^ {\prime} = F, x \mapsto (k \odot v)
$$

$$
\langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {   load   } (x , y . f)} \langle (r, S, F ^ {\prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-swap-temp 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = (v, F ^ {\prime}) \quad F ^ {\prime} (y) = (k, \iota) \quad \operatorname{load} (S, \iota) = o [ f \mapsto v ^ {\prime} ]
$$

$$
o ^ {\prime} = o [ f \mapsto v ] \quad \text { store } (S, \iota , o ^ {\prime}) = S ^ {\prime} \quad F ^ {\prime \prime} = F ^ {\prime}, x \mapsto v ^ {\prime}
$$

$$
\langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text { swap } (x , y . f , u s e)} \langle (r, S ^ {\prime}, F ^ {\prime \prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-swap-heap 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = (v, F ^ {\prime}) \quad F ^ {\prime} (y) = (k, \iota) \quad \operatorname{load} (S ^ {\prime}, \iota) = o [ f \mapsto v ^ {\prime} ]
$$

$$
o ^ {\prime} = o [ f \mapsto v ] \quad \text { store } (S ^ {\prime}, \iota , o ^ {\prime}) = S ^ {\prime \prime} \quad F ^ {\prime \prime} = F ^ {\prime}, x \mapsto v ^ {\prime}
$$

$$
\langle (r, S, F):: R S; (r, S ^ {\prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text { swap } (x , y . f , u s e)} \langle (r, S ^ {\prime}, F ^ {\prime \prime}):: R S; (r, S ^ {\prime \prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-alloc-heap-mut 

$$
x \text {   fresh   } \quad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = (v _ {i}, F _ {i + 1}) \quad \mathbf {f i e l d s} (C) = f _ {1}, \dots , f _ {n}
$$

$$
o = (\# C, [ f _ {1} \mapsto v _ {1}, \dots , f _ {n} \mapsto v _ {n} ]) \quad \iota f r e s h \quad S ^ {\prime \prime} = S ^ {\prime}, \iota \mapsto o \quad F ^ {\prime} = F _ {n + 1}, x \mapsto (\operatorname{mut}, \iota)
$$

$$
\langle (r, S, F _ {1}):: R S; (r, S ^ {\prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {halloc} (x , \text {mut} , \# C , u s e _ {1} \dots u s e _ {n})} \langle (r, S, F ^ {\prime}):: R S; (r, S ^ {\prime \prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-alloc-heap-iso 

$$
x \text {   fresh   } \quad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = (v _ {i}, F _ {i + 1}) \quad \mathbf {f i e l d s} (C) = f _ {1}, \dots , f _ {n}
$$

$$
o = \left(\# C, \left[ f _ {1} \mapsto v _ {1}, \dots , f _ {n} \mapsto v _ {n} \right]\right) \quad \iota f r e s h \quad r ^ {\prime} f r e s h \quad F ^ {\prime} = F _ {n + 1}, x \mapsto (\text { iso }, \iota)
$$

$$
\langle (r, S, F _ {1}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {halloc} (x , \text {iso} , \# C , u s e _ {1} \dots u s e _ {n})} \langle (r, S, F ^ {\prime}):: R S; H _ {o p}; (r ^ {\prime}, [ \iota \mapsto o ]) * H _ {c l}; H _ {f r} \rangle
$$

region-alloc-temp 

$$
x \text {   fresh   } \quad k \in \{\text { tmp }, \text { var } \} \quad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = (v _ {i}, F _ {i + 1}) \quad \mathbf {f i e l d s} (C) = f _ {1}, \dots , f _ {n}
$$

$$
o = \left(\# C L, \left[ f _ {1} \mapsto v _ {1}, \dots , f _ {n} \mapsto v _ {n} \right]\right) \quad \iota f r e s h \quad S ^ {\prime} = S [ \iota \mapsto o ] \quad F ^ {\prime} = F _ {n + 1}, x \mapsto (k, \iota)
$$

$$
\langle (r, S, F _ {1}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text { salloc } (x , k , \# C L , u s e _ {1} \dots u s e _ {n})} \langle (r, S ^ {\prime}, F ^ {\prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-freeze 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime}) \quad \quad \iota \in d o m (R. S)
$$

$$
H = \text { reachable\_regions } (R, (H * H _ {c l}) * H _ {o p}) \quad F ^ {\prime \prime} = F ^ {\prime}, x \mapsto (\text { imm }, \iota)
$$

$$
\langle (r, S, F):: R S; H _ {o p}; R * (H * H _ {c l}); H _ {f r} \rangle \xrightarrow {\text {freeze} (x , u s e)} \langle (r, S, F ^ {\prime \prime}):: R S; H _ {o p}; H _ {c l}; (R * H) * H _ {f r} \rangle
$$

region-merge 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime}) \quad \quad \iota \in d o m (R. S)
$$

$$
R ^ {\prime} = \left(r, S ^ {\prime} \uplus R. S\right) \quad F ^ {\prime \prime} = F ^ {\prime}, x \mapsto (\operatorname{mut}, \iota)
$$

$$
\langle (r, S, F):: R S; (r, S ^ {\prime}) * H _ {o p}; R * H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {merge} (x , u s e)} \langle (r, S, F ^ {\prime \prime}):: R S; R ^ {\prime} * H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-cast 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime})
$$

$$
\operatorname{cfg} _ {-} \text { load } ((r, S, F ^ {\prime}):: R S, H _ {o p} * H _ {c l} * H _ {f r}, \iota) = (\# C, \_)
$$

$$
F ^ {\prime \prime} = F ^ {\prime}, x \mapsto (k, \iota)
$$

$$
\langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text { cast } (x , u s e , k C)} \langle (r, S, F ^ {\prime \prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-nocast 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime})
$$

$$
\operatorname{cfg} _ {-} \text {load} ((r, S, F ^ {\prime}):: R S, H _ {o p} * H _ {c l} * H _ {f r}, \iota) = (\# C, \_)
$$

$$
F ^ {\prime \prime} = F ^ {\prime}, x \mapsto (k, \iota)
$$

$$
\langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {nocast} (x , u s e , k C)} \langle (r, S, F ^ {\prime \prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-enter-ok 

$$
w f r e s h \quad \forall i \in [ 1, n ]. z _ {i} f r e s h \quad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = ((k _ {i}, \iota_ {i}), F _ {i + 1})
$$

$$
\forall i \in [ 1, n ]. v _ {i} ^ {\prime} = \left\{ \begin{array}{l l} (k _ {i}, \iota_ {i}) & \text { if   } k _ {i} = \text { iso } \\ (\text { paused } \odot k _ {i}, \iota_ {i}) & \text { otherwise } \end{array} \right.
$$

$$
F = \left[ z _ {i} \mapsto v _ {i} ^ {\prime} \mid i \in [ 1, n ] \right] \quad F _ {n + 1} (y) = (\_, \iota) \quad \operatorname{cfg} _ {-} \operatorname{load} ((r, S, F _ {1}):: R S, H _ {o p}, \iota) = o [ f \mapsto (\_, \iota^ {\prime}) ]
$$

$$
\iota^ {\prime} \in d o m (S ^ {\prime}) \quad \iota^ {\prime \prime} f r e s h \quad F ^ {\prime} = F, w \mapsto (k, \iota^ {\prime \prime}) \quad R F = \left(r ^ {\prime}, \left[ \iota^ {\prime \prime} \mapsto (\# \text {Cell}, [ \operatorname{val} \mapsto (\operatorname{mut}, \iota^ {\prime}) ]) ], F ^ {\prime}\right) \right.
$$

$$
\langle (r, S, F _ {1}):: R S; H _ {o p}; (r ^ {\prime}, S ^ {\prime}) * H _ {c l}; H _ {f r} \rangle \xrightarrow {e n t e r (w , k , y . f , z _ {1} = u s e _ {1} , \dots , z _ {n} = u s e _ {n})} \langle R F:: (r, S, F _ {n + 1}):: R S; (r ^ {\prime}, S ^ {\prime}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-enter-fail 

$$
\forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = ((k _ {i}, \iota_ {i}), F _ {i + 1}) \quad F _ {n + 1} (y) = (\_, \iota)
$$

$$
\operatorname{cfg} _ {-} \text {load} ((r, S, F):: R S, H _ {o p}, \iota) = o [ f \mapsto (\_, \iota^ {\prime}) ] \quad \forall R ^ {\prime} \in H _ {c l}. \iota^ {\prime} \notin d o m (R ^ {\prime}. S)
$$

$$
\langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text { badenter } (y . f)} \langle (r, S, F):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-exit-temp 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F ^ {\prime}, u s e) = (v, F ^ {\prime \prime}) \quad F ^ {\prime \prime} (z) = (_ {-}, l ^ {\prime})
$$

$$
\operatorname{load} \left(S ^ {\prime}, \iota^ {\prime}\right) = o ^ {\prime} \left[ f ^ {\prime} \mapsto \left(\_, \iota^ {\prime \prime}\right) \right]
$$

$$
F (y) = (\_, \iota) \quad \text {   stack\_load } ((r, S, F):: R S, \iota) = o [ f \mapsto (k, \_) ]
$$

$$
\text { stack\_store } ((r, S, F):: R S, \iota , o [ f \mapsto (k, \iota^ {\prime \prime}) ]) = (r, S ^ {\prime \prime}, F):: R S ^ {\prime} \quad F ^ {\prime \prime \prime} = F, x \mapsto v
$$

$$
\langle (r ^ {\prime}, S ^ {\prime}, F ^ {\prime}):: (r, S, F):: R S; (r, S _ {o p}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {e x i t (x , u s e , y . f , z . f ^ {\prime})} \langle (r, S ^ {\prime \prime}, F ^ {\prime \prime \prime}):: R S ^ {\prime}; H _ {o p}; (r, S _ {o p}) * H _ {c l}; H _ {f r} \rangle
$$

region-exit-heap 

$$
x \text {   fresh   } \quad \mathbf {g e t} (F ^ {\prime}, u s e) = (v, F ^ {\prime \prime}) \quad F ^ {\prime \prime} (z) = (_ {-}, l ^ {\prime})
$$

$$
\operatorname{load} \left(S ^ {\prime}, \iota^ {\prime}\right) = o ^ {\prime} \left[ f ^ {\prime} \mapsto \left(\_, \iota^ {\prime \prime}\right) \right]
$$

$$
F (y) = (\_, \iota) \quad \text {   heap\_load } (H _ {o p}, \iota) = o [ f \mapsto (k, \_) ]
$$

$$
\mathsf {h e a p \_ s t o r e} (H _ {o p}, \iota , o [ f \mapsto (k, \iota^ {\prime \prime}) ]) = H _ {o p} ^ {\prime} \qquad F ^ {\prime \prime \prime} = F, x \mapsto v
$$

$$
\langle (r ^ {\prime}, S ^ {\prime}, F ^ {\prime}):: (r, S, F):: R S; (r ^ {\prime}, S _ {o p}) * H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {e x i t (x , u s e , y . f , z . f ^ {\prime})} \langle (r, S, F ^ {\prime \prime \prime}):: R S; H _ {o p} ^ {\prime}; (r ^ {\prime}, S _ {o p}) * H _ {c l}; H _ {f r} \rangle
$$

region-bind 

$$
\forall i \in [ 1, n ]. x _ {i} f r e s h \quad \forall i \in [ 1, n ]. \mathbf {g e t} (F _ {i}, u s e _ {i}) = (v _ {i}, F _ {i + 1}) \quad F ^ {\prime} = F _ {n + 1} + [ x _ {i} \mapsto v _ {i} \mid i \in [ 1, n ] ]
$$

$$
\langle (r, S, F _ {1}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {\text {bind} (x _ {1} = u s e _ {1} , \dots , x _ {n} = u s e _ {n})} \langle (r, S, F ^ {\prime}):: R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

region-eps 

$$
\overline {{r c f g \xrightarrow {\epsilon} r c f g}}
$$

# C.1.1 Helper Functions and Predicates. We use the following helper definitions:

Viewpoint adaptation of a value amounts to viewpoint adaptation on its capability (defined in Section E.2). 

$$
k \odot (k ^ {\prime} \iota) = (k \odot k ^ {\prime}) \iota
$$

We can look up objects in a store ?? given their identifier. 

$$
\begin{array}{r c l} \operatorname{load} ((\iota \mapsto o, S), \iota^ {\prime}) & = & \left\{ \begin{array}{l l} o & \text { if   } \iota = \iota^ {\prime} \\ \operatorname{load} (S, \iota^ {\prime}) & \text { otherwise } \end{array} \right. \\ \operatorname{load} (\varepsilon , \iota^ {\prime}) & = & \downarrow \end{array}
$$

Looking up an object on the region stack goes through the local stores of each frame from the top down. 

$$
\begin{array}{r c l} \text {stack\_load} ((r, S, F):: R S, \iota) & = & \left\{ \begin{array}{l l} o & \text {if load} (S, \iota) = o \\ \text {stack\_load} (S, \iota) & \text {otherwise} \end{array} \right. \\ \text {stack\_load} (\varepsilon , \iota) & = & \downarrow \end{array}
$$

Looking up an object on the heap goes through the regions left to right. 

$$
\begin{array}{r c l} \text {   heap\_load } (H _ {1} * H _ {2}, \iota) & = & \left\{ \begin{array}{l l} o & \text {   if   } \text {   heap\_load } (H _ {1}, \iota) = o \\ \text {   heap\_load } (H _ {2}, \iota) & \text {   otherwise   } \end{array} \right. \\ \text {   heap\_load } ((r, S), \iota) & = & \text {   load } (S, \iota) p \end{array}
$$

The top-level lookup function cfg_load performs a lookup on a given stack and heap using the helper functions above. Note that an object identifier ?? can only appear in the domain of a single store ?? in a well-formed configuration. 

$$
\operatorname{cfg} _ {-} \text { load } (R S, H, \iota) = \left\{ \begin{array}{l l} o & \text { if   } \text { stack\_load } (R S, \iota) = o \\ \text { heap\_load } (H, \iota) & \text { otherwise } \end{array} \right.
$$

Similarly, we can update a store ?? with a new object for a given object identifier. 

$$
\begin{array}{r c l} \text { store } ((\iota \mapsto o, S), \iota^ {\prime}, o ^ {\prime}) & = & \left\{ \begin{array}{l l} \iota \mapsto o ^ {\prime}, S & \text { if   } \iota = \iota^ {\prime} \\ \iota \mapsto o, \text { store } (S, \iota^ {\prime}, o ^ {\prime}) & \text { otherwise } \end{array} \right. \\ \text { store } (\varepsilon , \iota^ {\prime}, o ^ {\prime}) & = & \downarrow \end{array}
$$

When storing an object on the stack, we always store in the top-most frame, so we don’t need a function for traversing the stack. When storing to the heap, we go from left to right. 

$$
\begin{array}{r c l} \text {heap\_store} (H _ {1} * H _ {2}, \iota , o) & = & \left\{ \begin{array}{l l} H _ {1} ^ {\prime} * H _ {2} & \text {if heap\_store} (H _ {1}, \iota , o) = H _ {1} ^ {\prime} \\ H _ {1} * \text {heap\_store} (H _ {2}, \iota , o) & \text {otherwise} \end{array} \right. \\ \text {heap\_store} ((r, S), \iota , o) & = & \left\{ \begin{array}{l l} (r, S ^ {\prime}) & \text {if store} (S, \iota , o) = S ^ {\prime} \\ \downarrow & \text {otherwise} \end{array} \right. \end{array}
$$

When freezing a region, we need to calculate the reachable regions from the region being frozen. We define a relation reachable_step which holds when a region ?? has an object with a reference to an object in another region ??′. Calculating the reachable regions amounts to calculating the transitive closure of reachable_step. 

$$
\begin{array}{l} \text { reachable\_regions } (R, H) = \{R ^ {\prime} \mid \text { reachable\_step } ^ {+} (R, R ^ {\prime}) \} \\ \text { reachable\_step } (R, R ^ {\prime}) \equiv R ^ {\prime} \in H \land o [ f \mapsto \iota ] \in \mathbf {r n g} (R. S) \land \iota \in \mathbf {d o m} (R ^ {\prime}. S) \\ \end{array}
$$

# C.2 Well-Formedness Rules

$$
\overline {{\Gamma}} \vdash r c f g
$$

(Region configuration) 

wf-rcfg 

$$
\exists \Delta , \Psi . \overline {{\Gamma}}; \Delta ; \Psi \vdash r c f g
$$

$$
\text { capability\_ok } (\overline {{\Gamma}}, r c f g)
$$

$$
\frac {\text { topology\_ok } (\overline {{\Gamma}} , r c f g)}{\overline {{\Gamma}} \vdash r c f g}
$$

$$
\overline {{\Gamma}}; \Delta ; \Psi \vdash r c f g
$$

(Region configuration types) 

wf-ty-rcfg 

$$
\Delta_ {R S} \uplus \Delta_ {o p} \uplus \Delta_ {c l} \uplus \Delta_ {f r} = \Delta \quad \Psi_ {o p} \uplus \Psi_ {c l} \uplus \Psi_ {f r} = \Psi \quad \overline {{\Gamma}}; \Delta ; \Delta_ {R S}; \Psi_ {o p} \vdash R S
$$

$$
\Delta ; \Delta_ {o p}; \Psi_ {o p} \vdash H _ {o p} \qquad \Delta ; \Delta_ {c l}; \Psi_ {c l} \vdash H _ {c l} \qquad \Delta ; \Delta_ {f r}; \Psi_ {f r} \vdash H _ {f r}
$$

$$
\overline {{\Gamma}}; \Delta ; \Psi \vdash \langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle
$$

$$
\boxed {\overline {{\Gamma}}; \Delta ; \Delta ; \Psi \vdash R S}
$$

(Region stack types) 

wf-rs-cons 

$$
\Delta^ {\prime} = \Delta_ {1} \uplus \Delta_ {2} \quad \Delta ; \Delta_ {1} \vdash S
$$

$$
\Delta ; \Gamma \vdash F \qquad \overline {{\Gamma}}; \Delta ; \Delta_ {2}; \Psi \vdash R S
$$

$$
\Gamma :: \overline {{\Gamma}}; \Delta ; \Delta^ {\prime}; r, \Psi \vdash (r, S, F):: R S
$$

wf-rs-nil 

$$
\vdash \Delta
$$

$$
\varepsilon_ {\overline {{\Gamma}}}; \Delta ; \varepsilon_ {\Delta}; \varepsilon_ {\Psi} \vdash \varepsilon_ {R S}
$$

$$
\boxed {\Delta ; \Delta ; \Psi \vdash H}
$$

(Region heap types) 

wf-heap-prod 

$$
\Delta_ {1} \uplus \Delta_ {2} = \Delta^ {\prime} \quad \Psi_ {1} \uplus \Psi_ {2} = \Psi
$$

$$
\Delta ; \Delta_ {1}; \Psi_ {1} \vdash H _ {1} \qquad \Delta ; \Delta_ {2}; \Psi_ {2} \vdash H _ {2}
$$

$$
\Delta ; \Delta^ {\prime}; \Psi \vdash H _ {1} * H _ {2}
$$

wf-heap-sing 

$$
\Delta ; \Delta^ {\prime} \vdash S
$$

$$
\Delta ; \Delta^ {\prime}; r \vdash (r, S)
$$

$$
\boxed {\Delta ; \Delta \vdash S}
$$

(Region subheap types) 

wf-subheap-cons 

$$
\begin{array}{c} \iota : C L \in \Delta \qquad \# C L <   \#: C L \qquad \Delta ; \mathbf {f t y p e s} (C L) \vdash F \qquad \Delta ; \Delta^ {\prime} \vdash S \\ \hline \Delta ; \Delta^ {\prime}, \iota : C L \vdash S, \iota \mapsto (\# C L, F) \end{array}
$$

wf-subheap-nil 

$$
\Delta ; \varepsilon_ {\Delta} \vdash \varepsilon_ {S}
$$

$$
\boxed {\Delta ; \Gamma \vdash F}
$$

(Region variable types) 

wf-vars-cons 

$$
\frac {\iota : C L \in \Delta \qquad k   C L <   : t \qquad x \notin d o m (F)}{\Delta ; \Gamma , x : t \vdash F , x \mapsto (k , \iota)}
$$

wf-vars-nil 

$$
\Delta ; \varepsilon \vdash \varepsilon_ {F}
$$

$$
\boxed {\# C L <   \#: C L}
$$

(Subtagging) 

subtag-class 

$$
\overline {{\# C <   \#: C}}
$$

subtag-cell 

$$
\# \text { Cell } <   \#: \text { Cell } [ t ]
$$

# C.3 Predicates on Graphs

We will now define some predicates used to define the invariants of our system. These are defined in terms of the object graph of the configuration. Therefore we first define the set of graphs GRAPH, and a mapping ?? : RCFG → GRAPH, where RCFG is the set of region configurations. 

C.3.1 Region Configuration Graph. The the elements of GRAPH are tuples $( \mathcal { L } , \mathcal { R } )$ of vertices L (locations) and edges R (refereces). We represent the vertices and edges as 

$$
l o c \quad : := \quad \text { heap } (r, \iota) \mid \text { temp } (r, \iota) \mid \text { root } (r) \quad \text { Location }
$$

$$
r e f \quad : := \quad l o c \xrightarrow {f , k} l o c ^ {\prime}
$$

Reference, marked with field and capability 

We call the set of all locations and references LOCS and REFS respectively. 

$$
r i d (\operatorname{heap} (r, \iota)) = r
$$

$$
r i d (\operatorname{temp} (r, \iota)) = r
$$

$$
r i d (\operatorname{root} (r)) \quad = \quad r
$$

Since all loc contains a region id ?? , we can define a shorthand loc[·] with a hole for ?? as 

$$
l o c [ \cdot ]: := \text { heap } (\cdot , \iota) \mid \text { temp } (\cdot , \iota) \mid \text { root } (\cdot)
$$

We define ⊎ as the union of two disjoint sets. 

$$
A \uplus B = \left\{ \begin{array}{l l} A \cup B & \text { if } A \cap B = \emptyset \\ \downarrow & \text { otherwise } \end{array} \right.
$$

where ↓ denotes the undefined value. In particular, if any of ?? or ?? is ↓ we let $A \cap B = \downarrow$ , and thus ?? ⊎ $B = \downarrow$ . For sets $S , S ^ { \prime }$ , and ??, $B \subseteq S , f : S \to S ^ { \prime }$ , we define $\uplus \ v { f }$ by 

$$
A \uplus_ {f} B = \left\{ \begin{array}{l l} A \cup B & \text { if } I _ {f} (A) \cap I _ {f} (B) = \emptyset \\ \downarrow & \text { otherwise } \end{array} \right.
$$

where $I _ { f } ( A )$ is the image of ?? restricted to the set ??. Furthermore $I _ { f } ( \downarrow ) = \downarrow$ . We call ?? above a separating function. 

For the definition of ?? we define the separating function seploc as 

$$
\begin{array}{l} \operatorname{seploc} (\operatorname{heap} (r, \iota)) = \operatorname{obj} (\iota) \\ \operatorname{seploc} (\operatorname{temp} (r, \iota)) = \operatorname{obj} (\iota) \\ \operatorname{seploc} (\operatorname{root} (r)) \quad = \quad \operatorname{root} (r) \\ \end{array}
$$

Assuming $\mathcal { L } ^ { * } = \mathcal { L } \ \Theta _ { s e p l o c } \ \mathcal { L } ^ { \prime } \neq \downarrow$ we see that all object ids in $\mathcal { L } ^ { * }$ are unique, and furthermore that there is at most one root(?? ) for each region id ?? . 

We are now ready to start defining the graph of a region configuration rcfg. We write 

$$
G (\langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle) = (\mathcal {L}, \mathcal {R})
$$

where 

$$
\mathcal {L} = l o c _ {R S} (R S) \uplus_ {s e p l o c} l o c _ {H} (H _ {o p}) \uplus_ {s e p l o c} l o c _ {H} (H _ {c l}) \uplus_ {s e p l o c} l o c _ {H} (H _ {f r})
$$

$$
\begin{array}{l} l o c _ {R S} ((r, S, F):: R S) = l o c _ {S} ^ {\text { temp }} (S) \uplus_ {s e p l o c} \{\operatorname{root} (r) \} \uplus_ {s e p l o c} l o c _ {R S} (R S) \\ l o c _ {R S} (\varepsilon) \quad = \emptyset \\ \end{array}
$$

$$
\operatorname{loc} _ {H} \left(H _ {1} * H _ {2}\right) = \operatorname{loc} _ {H} \left(H _ {1}\right) \uplus_ {\text { seploc }} \operatorname{loc} _ {H} \left(H _ {2}\right)
$$

$$
l o c _ {H} ((r, S)) \quad = l o c _ {S} ^ {\text { heap }} (r; S)
$$

$$
\begin{array}{l} \operatorname{loc} _ {S} ^ {\text { heap }} (r; S, \iota \mapsto \_) = \operatorname{loc} _ {S} ^ {\text { heap }} (r; S) \uplus_ {\text { seploc }} \{\text { heap } (r, \iota) \} \\ \operatorname{loc} _ {S} ^ {\text {temp}} (r; S, \iota \mapsto \_) = \operatorname{loc} _ {S} ^ {\text {temp}} (r; S) \uplus_ {\text {seploc}} \{\operatorname{temp} (r, \iota) \} \\ l o c _ {S} ^ {*} (\varepsilon) \quad = \emptyset \\ \end{array}
$$

Because of the properties of seploc mentioned above (in particular the uniqueness of object ids $\iota ) ,$ , given that $\mathcal { L } \neq \downarrow$ we can define a partial function loc $\mathcal { L } : O i d \stackrel { } { - } \mathcal { L }$ as 

$$
l o c _ {\mathcal {L}} (\iota) = \left\{ \begin{array}{l l} \text { heap } (r, \iota) & \text { if   } \text { heap } (r, \iota) \in \mathcal {L} \\ \text { temp } (r, \iota) & \text { if   } \text { heap } (r, \iota) \in \mathcal {L} \end{array} \right.
$$

We move on to R. We define the separating function sepref as 

$$
\operatorname{sepref} \left(l o c \xrightarrow {f , k} l o c ^ {\prime}\right) = (l o c, f)
$$

We can use this as a separating function to ensure that uniqueness constraints on field names in a well-formed configuration will be mirrored in the construction of R. I.e. only configurations where each object or frame has at most one field with a certain name will have a well-defined graph. 

Given $l o c _ { \mathcal { L } }$ as above we have 

$$
\mathcal {R} = r e f _ {R S} (R S) \uplus^ {s e p r e f} r e f _ {H} (H _ {o p}) \uplus^ {s e p r e f} r e f _ {H} (H _ {c l}) \uplus^ {s e p r e f} r e f _ {H} (H _ {f r})
$$

$$
\begin{array}{l} {r e f _ {R S} ((r, S, F):: R S)} {=} {r e f _ {S} (S) \uplus^ {s e p r e f} r e f _ {F} (\operatorname{root} (r); F) \uplus^ {s e p r e f} r e f _ {R S} (R S)} \\ r e f _ {R S} (\varepsilon) = \emptyset \\ \end{array}
$$

$$
\operatorname{ref} _ {H} \left(H _ {1} * H _ {2}\right) = \operatorname{ref} _ {H} \left(H _ {1}\right) \uplus^ {\text { sepref }} \operatorname{ref} _ {H} \left(H _ {2}\right)
$$

$$
r e f _ {H} ((r, S)) = r e f _ {S} (S)
$$

$$
\operatorname{ref} _ {S} (S, \iota \mapsto (C L, F)) \quad = \operatorname{ref} _ {S} (S) \uplus^ {\text { sepref }} \operatorname{ref} _ {F} (\operatorname{loc} _ {\mathcal {L}} (\iota); F)
$$

$$
r e f _ {S} (\varepsilon) = \emptyset
$$

$$
{r e f _ {F} (l o c; F, f \mapsto (k, \iota))} = {r e f _ {F} (F) \uplus^ {s e p r e f} \{l o c \xrightarrow {f , k} l o c _ {\mathcal {L}} (\iota) \}}
$$

$$
{r e f _ {F} (l o c; F, f \mapsto \mathbf {u n d e f})} = {r e f _ {F} (F)}
$$

$$
r e f _ {F} (\varepsilon) \quad = \emptyset
$$

We want to have some kind of sanity check on a graph. Therefore we define well-formedness of a graph ${ \mathcal { G } } , W F G ( { \mathcal { G } } )$ as a predicate on a graph, as 

$$
W F G ((\mathcal {L}, \mathcal {R})) \quad \Longleftrightarrow \quad W F S r c D s t (\mathcal {L}, \mathcal {R}) \wedge W F R e f s (\mathcal {R})
$$

$$
W F S r c D s t (\mathcal {L}, \mathcal {R}) \quad \Longleftrightarrow \quad \forall l o c \xrightarrow {f , k} l o c ^ {\prime} \in \mathcal {R}.
$$

$$
l o c \in \mathcal {L} \land l o c ^ {\prime} \in \mathcal {L}
$$

$$
W F R e f s (\mathcal {R}) \quad \Longleftrightarrow \quad \forall r e f _ {1} = (l o c \xrightarrow {f , -} \_)  , r e f _ {2} = (l o c \xrightarrow {f ^ {\prime} , -} \_) \in \mathcal {R}.
$$

$$
r e f _ {1} = r e f _ {2} \vee f \neq f ^ {\prime}
$$

For a well-formed graph $\mathcal { G } = ( \mathcal { L } , \mathcal { R } )$ we can define the partial function 

$$
\operatorname{ref} _ {\mathcal {R}} (l o c, f) = \left\{ \begin{array}{l l} l o c \xrightarrow {f , k} l o c ^ {\prime} & \text { if } l o c \xrightarrow {f , k} l o c ^ {\prime} \in \mathcal {R} \\ \downarrow & \text { otherwise } \end{array} \right.
$$

Furthermore we define shorthands 

$$
l o c _ {\mathcal {G}} (\iota) \quad = l o c _ {\mathcal {L}} (\iota)
$$

$$
\operatorname{ref} _ {\mathcal {G}} (l o c, f) = \operatorname{ref} _ {\mathcal {R}} (l o c, f)
$$

$$
{r e f _ {\mathcal {G}} (\iota , f)} = {r e f _ {\mathcal {G}} (l o c _ {\mathcal {G}} (\iota), f)}
$$

C.3.2 Region Orders and Sets. We define $\rho$ to talk about ordering of regions. 

$$
\rho \quad : := \quad r \underset {x. f} {\::}: \rho \mid \varepsilon \quad \text {   Region   ordering   }
$$

If we do not care about $x . f$ we write a shorthand $r : : \rho .$ 

From a well formed region stack ${ \overline { { \Gamma } } } + R S ,$ , we can generate a $\rho \colon$ 

$$
\rho (\Gamma \underset {x. f} {\::}: \overline {{\Gamma}}, (r, S, F) \::: R S) = r \underset {x. f} {\::}: \rho (R S)
$$

$$
\rho (\varepsilon , \varepsilon) = \varepsilon
$$

We extend this definition to region configurations, $\rho ( { \overline { { \Gamma } } } , r c f g )$ , in the obvious way. 

We define ordering of region ids under $\rho$ as 

$$
\boxed {\rho \vdash r <   r ^ {\prime} \quad \rho \vdash r \leq r ^ {\prime}}
$$

(Region ordering) 

region-order-lt-cons 

$$
\frac {r \in \rho}{r ^ {\prime} : : _ {x . f} \rho \vdash r <   r ^ {\prime}}
$$

region-order-leq-strict 

$$
\frac {\rho \vdash r <   r ^ {\prime}}{\rho \vdash r \leq r ^ {\prime}}
$$

region-order-leq-eq 

$$
\overline {{\rho \vdash r \leq r}}
$$

We define 

$$
\text { Regions } \left(H _ {1} * H _ {2}\right) = \text { Regions } \left(H _ {1}\right) \uplus \text { Regions } \left(H _ {2}\right)
$$

$$
\text { Regions } ((r, S)) \quad = \quad \{r \}
$$

$$
\operatorname{Open} \left(\left\langle R S; H _ {o p}; H _ {c l}; H _ {f r} \right\rangle\right) = \text { Regions } \left(H _ {o p}\right)
$$

$$
{C l o s e d (\langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle)} = {R e g i o n s (H _ {c l})}
$$

$$
{F r o z e n (\langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle)} = {R e g i o n s (H _ {f r})}
$$

We define intersection of $\boldsymbol { \rho }$ and a set of region ids in the obvious way. The relation ⊢ $\rho ,$ Cl, Fr is simply disjointness of $\rho$ and the region id sets $C l , F r$ : 

$$
\vdash \rho , C l, F r \iff \rho \cap C l = \rho \cap F r = C l \cap F r = \emptyset
$$

C.3.3 Capability Semantics. The predicate capability $\underline { { \mathbf { o k } } } ( r c f \underline { { \mathbf { g } } } )$ is defined using three auxilliary predicates as follows. 

$$
\text { capability\_ok } (\overline {{\Gamma}}, r c f g) \iff \text { var\_unique } (G (r c f g)) \land
$$

$$
\text { region\_order } (\rho (\overline {{\Gamma}}, r c f g), \text { Closed } (r c f g), \text { Frozen } (r c f g), G (r c f g)) \land
$$

$$
\text { location\_ok } (G (r c f g))
$$

$$
\text { deep\_freeze } (F r o z e n (r c f g), G (r c f g))
$$

The three auxilliary predicates are defined using the object graph $G ( r c f g )$ as follows. 

$$
\text { region\_order } (\rho , C l, F r, \mathcal {G}) \iff \forall r e f \in \mathcal {G}. \text { region\_order1 } (\rho , C l, F r, r e f)
$$

$$
\text { region\_order1 } (\rho , C l, F r, (l o c [ r ] \xrightarrow {f , k} l o c ^ {\prime} [ r ^ {\prime} ]))
$$

⇐⇒ 

$$
(k = \text { mut } \quad \Longrightarrow r = r ^ {\prime}) \quad \wedge
$$

$$
(k = \operatorname{tmp} \quad \Longrightarrow r = r ^ {\prime}) \quad \wedge
$$

$$
(k = \operatorname{var} \quad \Longrightarrow r = r ^ {\prime}) \quad \wedge
$$

$$
(k = \text { paused } \quad \Longrightarrow \rho \vdash r ^ {\prime} <   r) \quad \wedge
$$

$$
(k = \text { iso } \quad \implies r \neq r ^ {\prime} \land (r ^ {\prime} \in C l \lor ((\rho \vdash r <   r ^ {\prime}) \lor r ^ {\prime} \in F r \land r \in F r))) \quad \land
$$

$$
(k = \operatorname{imm} \quad \Longrightarrow r ^ {\prime} \in F r))
$$

$$
\text { location\_ok } (\mathcal {G}) \iff \forall r e f \in \mathcal {G}. \text { location\_ok1 } (r e f)
$$

location_ok1( $loc \xrightarrow{f,k} loc'$ ) $\iff$ ( $k = \text{mut} \quad \Longrightarrow loc' = \text{heap}(r', \iota')$ )    ∧
    ( $k = \text{tmp} \quad \Longrightarrow ((loc = \text{root}(r) \lor loc = \text{temp}(r, \iota)) \land loc' = \text{temp}(r', \iota'))$ )    ∧
    ( $k = \text{var} \quad \Longrightarrow (loc = \text{root}(r) \land loc' = \text{temp}(r', \iota'))$ )    ∧
    ( $k = \text{paused} \quad \Longrightarrow ((loc = \text{root}(r) \lor loc = \text{temp}(r, \iota)) \land loc' \neq \text{root}(r'))$ )    ∧
    ( $k = \text{iso} \quad \Longrightarrow loc' = \text{heap}(r', \iota')$ )    ∧
    ( $k = \text{imm} \quad \Longrightarrow loc' = \text{heap}(r', \iota')$ ) $\quad$ deep_freeze(Fr, G) $\iff$ ∀ref ∈ G.deep_freeze1(Fr, ref) $\quad$ deep_freeze1(Fr, loc[r] $\stackrel{\cdot}{\longrightarrow}$ loc'[r']) $\iff$ r ∈ Fr $\implies$ r' ∈ Fr $\quad$ var_unique(G) $\iff$ ∀ref$_1$, Ref$_2$ ∈ G. $\quad$ var_pw_unique(ref$_1$, ref$_2$) $\quad$ var_pw_unique(ref$_1$, ref$_2$) $\iff$ ref$_1$ = $_-\xrightarrow{\_var}$ loc ∧ $\quad$ ref$_2$ = $_-\xrightarrow{\dot{\cdot}}$ loc $\implies$ ref$_1$ = ref$_2$ 

C.3.4 Topology Invariant. Finally we define topology_ok(rcfg). We again use the graph representation to formulate it. 

topology_ok( $\overline{\Gamma}$ , rcfg) $\Longleftrightarrow$ topology_ok_graph( $\rho(\overline{\Gamma}$ , rcfg), Frozen(rcfg), G(rcfg)) $\wedge$ entrypoints_ok( $\rho(\overline{\Gamma}$ , rcfg), G(rcfg))

topology_ok_graph( $\rho$ , Fr, G) $\Longleftrightarrow$ $\forall ref_{1}, ref_{2} \in G.$ topology_pw_ok( $\rho$ , Fr, ref $_{1}$ , ref $_{2}$ )

topology_pw_ok( $\rho$ , Fr, ref $_{1}$ , ref $_{2}$ ) $\Longleftrightarrow$ $ref_{1} = loc_{1}[r_{1}] \stackrel{\rightarrow}{\Longrightarrow} loc_{1}'[r_{1}'] \wedge$ $ref_{2} = loc_{2}[r_{2}] \stackrel{\rightarrow}{\Longrightarrow} loc_{2}'[r_{2}']$ $\Longrightarrow ref_{1} = ref_{2}$ ∨ $r_{1}' \neq r_{2}'$ ∨ $r_{1}' \in Fr \lor r_{2}' \in Fr$ ∨ $\rho \vdash r_{1}' \leq r_{1} \lor \rho \vdash r_{2}' \leq r_{2}$ 

Note that the topology invariant in the paper also relies on the source of backward pointing references being temp locations. This is not included in this version, but still holds because of capability_ok. 

$\text{entrypoints\_ok}(r' :: r :: \rho, \mathcal{G}) \Longleftrightarrow$ $\text{root}(r) \xrightarrow{x, k_x} loc_x[r^*] \in \mathcal{G} \wedge$ $loc_x[r^*] \xrightarrow{f, k_f} \text{heap}(r',_) \in \mathcal{G} \wedge$ $\text{entrypoints\_ok}(r :: \rho, \mathcal{G})$ $\text{entrypoints\_ok}(r :: \varepsilon, \mathcal{G}) \Longleftrightarrow \text{true}$ 


D EFFECTS


use ::= x | drop x
Eff ::= load(x, y.f)
| swap(x, y.f, use)
| bind(x = use)
| halloc(x, k, #C, $\overline{use}$ )
| salloc(x, k, #CL, $\overline{use}$ )
| enter(w, k, y.f, CL, $\overline{x = use}$ )
| badenter(x.f)
| exit(x, use, y.f, w.g)
| cast(x, use, k C)
| nocast(x, use, k C)
| bind( $\overline{x = use}$ )
| $\epsilon$ 

# D.1 Well-Formed Effects

We use the following helper predicate for a capability pointing into an open region: 

$$
\mathbf {o p e n} (k) \iff k \in \{\text { mut }, \text { tmp }, \text { var }, \text { paused } \}
$$

Effects are typed under a stack of contexts ${ \overline { { \Gamma } } } ,$ defined in Section E.2. 

$$
\overline {{\Gamma}} \vdash E f f + \overline {{\Gamma}}
$$

(Well-formed Effects) 

WF-EFF-ENTER $wfresh \quad \forall i \in [1, n]. x_i fresh \quad \forall i \in [1, n]. \Gamma_i \vdash use_i : t_i \dashv \Gamma_{i+1}$ $\Gamma_{n+1}(y) = k' CL \quad \text{open}(k') \quad \mathbf{ftype}(CL, f) = t \quad \text{cap(iso, } t)$ $k \in \{\text{tmp}, \text{var}\} \quad k = \text{var} \Rightarrow k' = \text{var} \quad t' = \text{make\_mut}(t)$ $\Gamma = y_1 : t_1', ..., y_n : t_n' \text{ where } t_i' = \begin{cases} t_i & \text{if cap(iso, } t_i) \\ \text{paused} \odot t_i & \text{otherwise} \end{cases}$ $\Gamma' = \Gamma, z : k \text{ Cell}[t']$ 

$$
\Gamma_ {1}:: \overline {{\Gamma}} \vdash e n t e r (w, k, y. f, x _ {1} = u s e _ {1}, \dots , x _ {n} = u s e _ {n}) \dashv \Gamma^ {\prime}:: \Gamma_ {n + 1}:: \overline {{\Gamma}}
$$

WF-EFF-BADENTER $\Gamma(y) = k CL$ open(k) $\mathbf{ftype}(CL,f) = t$ cap(iso, t) $\Gamma_{1} :: \overline{\Gamma} \vdash badenter(y.f) + \Gamma_{1} :: \overline{\Gamma}$ 

wf-eff-exit 

$$
x \text {   fresh   } \quad \text { open } (k) \quad \Gamma^ {\prime} \vdash u s e: t \dashv \Gamma^ {\prime \prime} \quad \text { cap } (\{\text { iso }, \text { imm } \}, t)
$$

$$
\Gamma^ {\prime \prime} (w) = k ^ {\prime} C L ^ {\prime} \quad \text { f   t   y   p   e } \left(C L ^ {\prime}, g\right) = t ^ {\prime} \quad \operatorname{cap} (\operatorname{mut}, t ^ {\prime})
$$

$$
k ^ {\prime} \in \{\text { tmp }, \text { var } \} \quad \text { cap } (\text { iso }, \text { ftype } (C L, f))
$$

$$
k ^ {\prime \prime} C L ^ {\prime \prime} = \left\{ \begin{array}{l l} \text { var   Cell } [ m a k e \_ i s o (t ^ {\prime}) ] & \text { if   } k C L = \text { var   Cell } [ \_ ] \\ k C L & \text { otherwise } \end{array} \right.
$$

$$
\Gamma^ {\prime} \underset {y. f} {\::} \Gamma [ y: k   C L ]:: \overline {{\Gamma}} \vdash e x i t (x, u s e, y. f, w. g) \dashv \Gamma [ y: k ^ {\prime \prime}   C L ^ {\prime \prime} ], x: t:: \overline {{\Gamma}}
$$

wf-eff-load 

$$
x \text {   fresh   } \quad \Gamma (y) = k C L \quad \text {   ftype   } (C L, f) = t
$$

$$
k \neq \text { iso } \quad \vdash k \odot t
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash l o a d (x, y. f) \dashv \Gamma , x: k \odot t:: \overline {{\Gamma}}
$$

wf-eff-swap-class 

$$
x \text {   fresh   } \quad \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \quad \text { cap } (\{\text { var } \} ^ {c}, t) \quad \Gamma^ {\prime} (y) = k C L
$$

$$
k \in \{\text { mut }, \text { tmp } \} \quad \text { ftype } (C L, f) = t
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash s w a p (x, y. f, u s e) \dashv \Gamma^ {\prime}, x: t:: \overline {{\Gamma}}
$$

wf-eff-swap-var 

$$
x \text {   fresh   } \quad \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} [ y: \text { var   Cell } [ t ^ {\prime} ] ] \quad \text { cap } (\{\text { var } \} ^ {c}, t)
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash s w a p (x, y. v a l, u s e) \dashv \Gamma^ {\prime} [ y: \operatorname{varCell} [ t ] ], x: t ^ {\prime}:: \overline {{\Gamma}}
$$

wf-eff-halloc-mut 

$$
x \text {   fresh   } \quad \vdash \text {   mut   } C \quad \text {   ftypes   } (C) = f _ {1}: t _ {1}, \dots , f _ {n}: t _ {n}
$$

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1}
$$

$$
\Gamma_ {1}:: \overline {{\Gamma}} \vdash h a l l o c (x, \text { mut }, \# C, u s e _ {1}, \dots , u s e _ {n}) \dashv \Gamma_ {n + 1}, x: \text { mut } C:: \overline {{\Gamma}}
$$

wf-eff-halloc-iso 

$$
x \text {   fresh   } \quad \vdash \text {   iso   } C \quad \text {   ftypes   } (C) = f _ {1}: t _ {1}, \dots , f _ {n}: t _ {n}
$$

$$
\forall i \in [ 1, n ]. (\operatorname{cap} (\{\text {iso}, \text {imm} \}, t _ {i} ^ {\prime}) \wedge \Gamma_ {i} \vdash u s e _ {i}: t _ {i} ^ {\prime} \dashv \Gamma_ {i + 1} \wedge t _ {i} ^ {\prime} <  : t _ {i})
$$

$$
\Gamma_ {1}:: \overline {{\Gamma}} \vdash h a l l o c (x, \text {iso}, \# C, u s e _ {1}, \dots , u s e _ {n}) \dashv \Gamma_ {n + 1}, x: \text {iso} C:: \overline {{\Gamma}}
$$

wf-eff-salloc 

$$
x \text {   fresh   } \quad k \in \{\text { tmp }, \text { var } \} \quad \vdash k C L \quad \text {   ftypes   } (C) = f _ {1}: t _ {1}, \dots , f _ {n}: t _ {n}
$$

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1}
$$

$$
\Gamma_ {1}:: \overline {{\Gamma}} \vdash s a l l o c (x, k, \# C L, u s e _ {1}, \dots , u s e _ {n}) \dashv \Gamma_ {n + 1}, x: k C L:: \overline {{\Gamma}}
$$

wf-eff-freeze 

$$
x \text {   fresh   } \quad \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \quad \text { cap(iso,   } t)
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash f r e e z e (x, u s e) \dashv \Gamma^ {\prime}, x: m a k e \_ i m m (t):: \overline {{\Gamma}}
$$

wf-eff-merge 

$$
x \text {   fresh   } \quad \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \quad \text { cap(iso,   } t)
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash m e r g e (x, u s e) \dashv \Gamma^ {\prime}, x: m a k e \_ m u t (t):: \overline {{\Gamma}}
$$

wf-eff-cast 

$$
x \text {   fresh   } \quad \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \quad k C <  : t ^ {\prime}
$$

$$
\Gamma :: \overline {{\Gamma}} \vdash c a s t (x, u s e, k C) \dashv \Gamma^ {\prime}, x: t ^ {\prime}:: \overline {{\Gamma}}
$$

$$
\frac {\text { WF - EFF - NOCAST } \quad x   \text { fresh } \quad \Gamma \vdash u s e : t \dashv \Gamma^ {\prime}}{\Gamma : : \overline {{\Gamma}} \vdash n o c a s t (x , u s e , k C) \dashv \Gamma^ {\prime} , x : t : : \overline {{\Gamma}}}
$$

wf-eff-bind 

$$
\frac {\forall i \in [ 1, n ] . x _ {i} f r e s h \quad \forall i \in [ 1 , n ] . \Gamma_ {i} \vdash u s e _ {i} : t _ {i} \dashv \Gamma_ {i + 1}}{\Gamma_ {1} : : \overline {{\Gamma}} \vdash b i n d (x _ {1} = u s e _ {1} , \dots , x _ {n} = u s e _ {n}) \dashv \Gamma_ {n + 1} , x _ {1} : t _ {1} , \dots , x _ {n} : t _ {n} : : \overline {{\Gamma}}}
$$

wf-eff-split 

$$
\frac {\overline {{\Gamma}} [ x : t ] \vdash E f f \dashv \overline {{\Gamma}} _ {1} \qquad \overline {{\Gamma}} [ x : t ^ {\prime} ] \vdash E f f \dashv \overline {{\Gamma}} _ {2}}{\overline {{\Gamma}} [ x : t | t ^ {\prime} ] \vdash E f f \dashv \overline {{\Gamma}} _ {1} | \overline {{\Gamma}} _ {2}} \qquad \qquad \begin{array}{l} \text {WF - EFF - EPS} \\ \overline {{\Gamma}} \vdash \epsilon \dashv \overline {{\Gamma}} \end{array}
$$

# E COMMAND SEMANTICS

$$
\begin{array}{l} e \quad : := \quad u s e \mid \text { let } x = b \text { in } e \mid \text { if   } \text { typetest } (u s e, t) \{y = > e \} \{y = > e \} \quad \text { Expressions } \\ u s e \quad : := \quad x \mid \text { drop } x \quad \text { Variable   usage } \\ \begin{array}{l l} b & := \quad * l v a l \mid l v a l := u s e \mid f n c (\overline {{u s e}}) \mid \text {var use} \mid \text {new k C} (\overline {{u s e}}) \\ & \mid \quad \text {enter lval [y = use] \{z = > e\} | freeze use | merge use} \mid e \end{array} \qquad \text {Bound expressions} \\ l v a l \quad : := \quad x \mid x. f \quad \text { L - values } \\ d e \quad : := \quad e \mid \text { let } x = d b \text { in } e \mid \text { Failure } \quad \text { Dynamic   expressions } \\ d b \quad : := \quad b \mid d e \mid \text { entered } x. f y. f ^ {\prime} \{d e \} \quad \text { Dynamic   bound   expressions } \\ d e [ \bullet ] := \bullet | \text { let } x = \bullet \text { in } e | \text { let } x = \text { entered } y. f w. f ^ {\prime} \{\bullet \} \text { in } e \\ c c f g \quad : := \quad \{d e \} \quad \text {Dynamic configuration} \\ \end{array}
$$

$$
\begin{array}{l} d e [ \bullet ] := \bullet | \text { let } x = \bullet \text { in } e | \text { let } x = \text { entered } y. f w. f ^ {\prime} \{\bullet \} \text { in } e \\ c c f g \quad : := \quad \{d e \} \quad \text {Dynamic configuration} \\ \end{array}
$$

The syntax of the command language is in A-normal form (ANF). An expression is a use of a name (a potentially destructive read), the binding of an expression ?? to a name, or a dynamic type test, where the value is rebound in the chosen branch in order to track the type. Note that we model mutable variables as objects, created with the syntax var use. A bound expression is a field access, a field update, a function call, the creation of a variable or an object, an enter block, or a freeze or merge. An enter block enters through a variable ?? or a field $x . f$ and explicitly captures variables use, potentially destructively, as ${ \overline { { y } } } .$ The bridge object is bound to ?? and the body ?? of the block is executed. The capture list is for convenience only, and could be inferred based on variable usage in ?? . 

During execution, an enter block reduces to a entered block, which stores the field entered through and the field in which the new bridge object can be found when exiting the region. The only difference between dynamic expressions ???? and static expressions ?? is that dynamic expressions can contain entered blocks in the currently bound expression being evaluated, or denote failure. We use an execution context $d e [ \bullet ]$ to find the inner-most dynamic expression to evaluate. A dynamic configuration is just a dynamic expression. 

We assume the existence of a table of well-formed functions such that if fnctype $( f n c ) ~ =$ $( t _ { 1 } , . . . , t _ { n } )  t$ , then fnclookup $( f n c ) = ( x _ { 1 } , . . . , x _ { n } )  e$ and $x _ { 1 } : t _ { 1 } , . . . , x _ { n } : t _ { n } \vdash e : t + \Gamma ^ { \prime }$ for some $\Gamma ^ { \prime }$ . We further assume that function lookup renames all let-bound variables to globally fresh names to avoid accidental shadowing. 

# E.1 Dynamic Semantics

Because the command language is in ANF, many rules differ only in which effect they emit. In order to avoid duplicating rules, for expressions with the shape let $\boldsymbol { x } ~ = ~ \boldsymbol { e } _ { 1 }$ in $e _ { 2 }$ we define a helper 

function to calculate the effect of ??: 

effect(x,*y.f) = load(x,y.f)
effect(x,*y) = load(x,y.val)
effect(x,y.f:=use) = swap(x,y.f, use)
effect(x,y:=use) = swap(x,y.val, use)
effect(x,new mut C( $\overline{use}$ )) = malloc(x,mut,#C, $\overline{use}$ )
effect(x,new iso C( $\overline{use}$ )) = malloc(x,iso,#C, $\overline{use}$ )
effect(x,new tmp C( $\overline{use}$ )) = salloc(x,tmp,#C, $\overline{use}$ )
effect(x,var use) = salloc(x,var,#Cell, use)
effect(x,freeze use) = freeze(x, use)
effect(x,merge use) = merge(x, use)
effect(x,use) = bind(x=use) 

$$
\boxed {d e \xrightarrow {E f f} d e}
$$

(Command semantics) 

CMD-LET-EFF $x'$ fresh $\mathbf{effect}(x', db) = Eff$ let x = db in $e \xrightarrow{Eff} e[x \mapsto x']$ 

cmd-enter-field-fail 

$\mathbf { l e t } \ x = \mathbf { e n t e r } \ y . f \ [ z _ { 1 } = u s e _ { 1 } , \ldots , z _ { n } = u s e _ { n } ] \ \{ w = > e \} \ \mathbf { i n } \ e ^ { \prime } \ \xrightarrow [ ] { b a d e n t e r ( y , f ) } \ \mathbf { F a i l u r e } \ \mathbf { e }$ 

cmd-enter-var-fail 

$\mathbf { l e t } \ x = \mathbf { e n t e r } \ y \left[ z _ { 1 } = u s e _ { 1 } , \ldots , z _ { n } = u s e _ { n } \right] \ \{ w = > e \} \ \mathbf { i n } \ e ^ { \prime } \ \xrightarrow [ ] { b a d e n t e r ( y , \mathbf { v } \mathbf { a } 1 ) } \ \mathbf { F a i l u r e }$ 

cmd-enter-field 

$$
d b _ {1} = \text { enter   } y. f [ z _ {1} = u s e _ {1}, \dots , z _ {n} = u s e _ {n} ] \{w = > e \}
$$

$$
d b _ {2} = \text { entered   } y. f w ^ {\prime}. \text { val } \left\{e [ w \mapsto w ^ {\prime}, z _ {1} \mapsto z _ {1} ^ {\prime}, \dots , z _ {n} \mapsto z _ {n} ^ {\prime} ] \right\}
$$

$$
w ^ {\prime} \text {   fresh   } \quad z _ {1} ^ {\prime}, \dots , z _ {n} ^ {\prime} \text {   fresh   }
$$

$\mathbf { l e t } \ x = d b _ { 1 } \ \mathbf { i n } \ e ^ { \prime } \ { \frac { \ e n t e r \ ( w ^ { \prime } , \mathbf { t m p } , y . f , z _ { 1 } ^ { \prime } = u s e _ { 1 } , \dots , z _ { n } ^ { \prime } = u s e _ { n } ) } { 1 } } \ \mathbf { l e t } \ x = d b _ { 2 } \ \mathbf { i n } \ e ^ { \prime }$ 

cmd-enter-var 

$$
d b _ {1} = \text { enter   } y \left[ z _ {1} = u s e _ {1}, \dots , z _ {n} = u s e _ {n} \right] \{w \Rightarrow e \}
$$

$$
d b _ {2} = \text { entered   } y. f w ^ {\prime}. \text { val } \left\{e [ w \mapsto w ^ {\prime}, z _ {1} \mapsto z _ {1} ^ {\prime},..., z _ {n} \mapsto z _ {n} ^ {\prime} ] \right\}
$$

$$
w ^ {\prime} \text {   fresh   } \quad z _ {1} ^ {\prime}, \dots , z _ {n} ^ {\prime} \text {   fresh   }
$$

$\mathbf { l e t } \ x = d b _ { 1 } \ \mathbf { i n } \ e ^ { \prime } \ { \frac { \ e n t e r \ ( w ^ { \prime } , \mathbf { v a r } , y . \mathbf { v a l } , z _ { 1 } ^ { \prime } = u s e _ { 1 } , \dots , z _ { n } ^ { \prime } = u s e _ { n } ) } { \mathbf { l e t } \ x = d b _ { 2 } \ \mathbf { i n } \ e ^ { \prime } } } \ \mathbf { l e t } \ x = d b _ { 2 } \ \mathbf { i n } \ e ^ { \prime }$ 

cmd-exit 

$$
x ^ {\prime} f r e s h
$$

$\mathbf { l e t } \ x = \mathbf { e n t e r e d } \ y . f \ w ^ { \prime } . v \mathbf { a l } \ \{ u s e \} \ \mathbf { i n } \ e \ \xrightarrow [ ] { e x i t \ ( x ^ { \prime } , u s e , y . f , w ^ { \prime } . v \mathbf { a l } ) } \ e [ x \mapsto x ^ { \prime } ]$ 

cmd-if-typetest-true 

$$
k C L <  : t \quad y ^ {\prime} f r e s h
$$

${ \bf i f ~ t y p e t e s t ~ ( } u s e , t { \bf ) \ ( } \ y { \mathsf { = > } } e _ { 1 }  \mathsf { \} } \ \xi \ \qquad \Longleftrightarrow e _ { 2 } { \mathsf { \ } } \ \xi \ { \xrightarrow { { \scriptstyle c a s t ( } y ^ { \prime } , u s e , k \ C L { \mathsf { ) } } } } \ e _ { 1 } [ y \mapsto y ^ { \prime } ]$ 

cmd-if-typetest-false 

$$
k C L \not \prec : t \quad y ^ {\prime} f r e s h
$$

$$
\text { if   } \text { typetest } (u s e, t) \{y = > e _ {1} \} \{y = > e _ {2} \} \xrightarrow {\text { nocast } (y ^ {\prime} , u s e , k C L)} e _ {2} [ y \mapsto y ^ {\prime} ]
$$

cmd-call 

$$
\mathbf {f n c l o o k u p} (f n c) = \left(x _ {1}, \dots , x _ {n}\right)\rightarrow e _ {\text { body }} \quad x _ {1} ^ {\prime}, \dots , x _ {n} ^ {\prime} \text {   fresh }
$$

$$
\overline {{x}} = x _ {1}, \ldots , x _ {n} \qquad \overline {{x}} ^ {\prime} = x _ {1} ^ {\prime}, \ldots , x _ {n} ^ {\prime}
$$

$$
\text { let } x = f n c (u s e _ {1}, \dots , u s e _ {n}) \text { in } e \xrightarrow {\text { bind } (x _ {1} ^ {\prime} = u s e _ {1} , \dots , x _ {n} ^ {\prime} = u s e _ {n})} \text { let } x = e _ {b o d y} [ \overline {{x}} \mapsto \overline {{x}} ^ {\prime} ] \text { in } e
$$

cmd-ec 

$$
\frac {d e ^ {\prime} \xrightarrow {E f f} d e ^ {\prime \prime}}{d e [ d e ^ {\prime} ] \xrightarrow {E f f} d e [ d e ^ {\prime \prime} ]} \qquad \qquad \begin{array}{l} \text {CMD - EC - FAIL} \\ \hline d e [ \text {Failure} ] \xrightarrow {\epsilon} \text {Failure} \end{array}
$$

# E.2 Static Semantics

$$
t \quad : := \quad k C \mid k \operatorname{Cell} [ t ] \mid t | t \quad \text { Types }
$$

$$
\Gamma \quad : := \quad \Gamma , x: t \mid \Gamma , x: \mathbf {u n d e f} \mid \varepsilon \quad \text { Typing   context }
$$

$$
\overline {{\Gamma}} \quad := = \quad \overline {{\Gamma}} _ {x. f}:: \Gamma \mid \varepsilon \quad \text {   Dynamic   typing   context   }
$$

A type is a class ?? with a capability ??, a Cell with a given type $( c . f .$ , ML-style ref cells), or a union $t _ { 1 } | t _ { 2 }$ of two types. A typing context maps variables to types, or remembers that they have been consumed. When typing a dynamic expression, we use a stack of typing contexts mirroring the stack of open regions. Each :: between two contexts in the stack is marked with $x . f$ denoting the field through which we entered the next region. Whenever we don’t care about this field, we omit it for readability. When dealing with union types, we sometimes need to merge two different contexts with the same domain but (possibly) different codomains. This is done with the following function: 

$$
(\Gamma_ {1} | \Gamma_ {2}) = \left\{ \begin{array}{l l} (\Gamma_ {1} ^ {\prime} | \Gamma_ {2} ^ {\prime}), x: t _ {1} | t _ {2} & \text {if} \Gamma_ {1} = \Gamma_ {1} ^ {\prime}, x: t _ {1} \quad \text {and} \quad \Gamma_ {2} = \Gamma_ {2} ^ {\prime}, x: t _ {2} \\ (\Gamma_ {1} ^ {\prime} | \Gamma_ {2} ^ {\prime}), x: \mathbf {u n d e f} & \text {if} \Gamma_ {1} = \Gamma_ {1} ^ {\prime}, x: \mathbf {u n d e f} \quad \text {and} \quad \Gamma_ {2} = \Gamma_ {2} ^ {\prime}, x: t _ {2} \\ (\Gamma_ {1} ^ {\prime} | \Gamma_ {2} ^ {\prime}), x: \mathbf {u n d e f} & \text {if} \Gamma_ {1} = \Gamma_ {1} ^ {\prime}, x: t _ {1} \quad \text {and} \quad \Gamma_ {2} = \Gamma_ {2} ^ {\prime}, x: \mathbf {u n d e f} \\ (\Gamma_ {1} ^ {\prime} | \Gamma_ {2} ^ {\prime}), x: \mathbf {u n d e f} & \text {if} \Gamma_ {1} = \Gamma_ {1} ^ {\prime}, x: \mathbf {u n d e f} \quad \text {and} \quad \Gamma_ {2} = \Gamma_ {2} ^ {\prime}, x: \mathbf {u n d e f} \\ \varepsilon & \text {if} \Gamma_ {1} = \varepsilon \quad \text {and} \quad \Gamma_ {2} = \varepsilon \end{array} \right.
$$

$$
(\overline {{\Gamma}} _ {1} | \overline {{\Gamma}} _ {2}) = \left\{ \begin{array}{l l} (\Gamma_ {1} ^ {\prime} | \Gamma_ {2} ^ {\prime}):: (\overline {{\Gamma}} _ {1} ^ {\prime} | \overline {{\Gamma}} _ {2} ^ {\prime}) & \text {if} \overline {{\Gamma}} _ {1} = \Gamma_ {1} ^ {\prime}: \overline {{\Gamma}} _ {1} ^ {\prime} \quad \text {and} \quad \overline {{\Gamma}} _ {2} = \Gamma_ {2} ^ {\prime}: \overline {{\Gamma}} _ {2} ^ {\prime} \\ \varepsilon & \text {if} \overline {{\Gamma}} _ {1} = \varepsilon \quad \text {and} \quad \overline {{\Gamma}} _ {2} = \varepsilon \end{array} \right.
$$

We sometimes need to compare what variables are defined for a certain Γ or ${ \overline { { \Gamma } } } .$ . We use define defined as follows 

$$
\mathbf {d e f i n e d} (\Gamma , x: t) \quad = \quad \mathbf {d e f i n e d} (\Gamma) \cup \{x \}
$$

$$
\mathbf {d e f i n e d} (\Gamma , x: \mathbf {u n d e f}) = \mathbf {d e f i n e d} (\Gamma)
$$

$$
\mathbf {d e f i n e d} (\varepsilon) \quad = \emptyset
$$

Reference Capabilities for Flexible Memory Management: Extended Version 

Table 4. Viewpoint adaptation. If the capabilities of x and f are ?? and $\beta ,$ then the capability of x.f is $\alpha \odot \beta = \gamma ,$ , which we read as “?? sees $\beta$ as $\gamma . ^ { \mathfrak { n } }$ Note that this table is slightly different from the one in the paper since we only use it for non-destructive reads. 

<table><tr><td rowspan="2">Capability on x</td><td colspan="5">Capability on f</td></tr><tr><td>mut</td><td>tmp</td><td>imm</td><td>iso</td><td>paused</td></tr><tr><td>mut</td><td>mut</td><td>↓</td><td>imm</td><td>↓</td><td>↓</td></tr><tr><td>tmp</td><td>mut</td><td>tmp</td><td>imm</td><td>↓</td><td>paused</td></tr><tr><td>var</td><td>mut</td><td>tmp</td><td>imm</td><td>↓</td><td>paused</td></tr><tr><td>imm</td><td>imm</td><td>imm</td><td>imm</td><td>imm</td><td>imm</td></tr><tr><td>iso</td><td>↓</td><td>↓</td><td>↓</td><td>↓</td><td>↓</td></tr><tr><td>paused</td><td>paused</td><td>paused</td><td>imm</td><td>↓</td><td>paused</td></tr></table>

$$
\begin{array}{l l l} k \odot (k ^ {\prime} C L) & = & \left\{ \begin{array}{l l} k ^ {\prime \prime} C L & \text {if} k \odot k ^ {\prime} = k ^ {\prime \prime} \\ \downarrow & \text {otherwise} \end{array} \right. \\ k \odot (t _ {1} \mid t _ {2}) & = & \left\{ \begin{array}{l l} t _ {1} ^ {\prime} \mid t _ {2} ^ {\prime} & \text {if} k \odot t _ {1} = t _ {1} ^ {\prime} \text {and} k \odot t _ {2} = t _ {2} ^ {\prime} \\ \downarrow & \text {otherwise} \end{array} \right. \end{array}
$$

To compare $\overline { { \Gamma } } _ { 1 }$ and $\overline { { \Gamma } } _ { 2 }$ , we check pairwise set inclusion of defined sets: 

$$
\begin{array}{l} \Gamma :: \overline {{\Gamma}} \stackrel {\mathrm{pw}} {\subseteq} \Gamma^ {\prime}:: \overline {{\Gamma}} ^ {\prime} \iff \\ \mathbf {d e f i n e d} (\Gamma) \subseteq \mathbf {d e f i n e d} (\Gamma^ {\prime}) \wedge \\ \overline {{\Gamma}} \stackrel {\mathrm{pw}} {\subseteq} \overline {{\Gamma}} ^ {\prime} \\ \end{array}
$$

We further use the following helper functions and predicates (viewpoint adaptation is in Table 4): 

We use make_iso and make_mut to change all occurrences of mut to iso and vice versa for a type (all other capabilities remain unchanged). 

$$
\begin{array}{l} \text { make\_iso } (k C L) = \left\{ \begin{array}{l l} \text { iso   } C L & \text { if   } k = \text { mut } \\ k C L & \text { otherwise } \end{array} \right. \\ \text { make\_iso } (t _ {1} \mid t _ {2}) = \text { make\_iso } (t _ {1}) \mid \text { make\_iso } (t _ {2}) \\ \end{array}
$$

$$
\text { make\_mut } (k C L) = \left\{ \begin{array}{l l} \text { mut } C L & \text { if } k = \text { iso } \\ k C L & \text { otherwise } \end{array} \right.
$$

$$
\text { make\_mut } (t _ {1} \mid t _ {2}) = \text { make\_mut } (t _ {1}) \mid \text { make\_mut } (t _ {2})
$$

We use the predicate $\mathsf { c a p } ( \overline { { k } } , t )$ to check if all capabilities in a type are in ${ \overline { { k } } } .$ We omit brackets for singleton sets (the first case below), and use the complement notation cap $\langle \mathit { l k } \} ^ { c } , t \rangle$ for checking that capability ?? does not occur anywhere in ??. 

$$
\begin{array}{l} \operatorname{cap} (k, t) \equiv \operatorname{cap} (\{k \}, t) \\ \operatorname{cap} (\overline {{k}}, k ^ {\prime} C L) \quad \Longleftrightarrow \quad k ^ {\prime} \in \overline {{k}} \\ \operatorname{cap} (\overline {{k}}, t _ {1} \mid t _ {2}) \quad \Longleftrightarrow \quad \operatorname{cap} (\overline {{k}}, t _ {1}) \wedge \operatorname{cap} (\overline {{k}}, t _ {2}) \\ \end{array}
$$

We use classtype(??) to check if a type is definitely a class type: 

$$
\mathbf {c l a s t y p e} (t) \iff \forall k C L \in t. \exists C. C L = C
$$

We extend viewpoint adaptation to ?? ⊙ Γ and ?? ⊙ ?? by mapping over the elements of Γ and ?? respectively. 

$$
\boxed {\Gamma \vdash e \dashv \Gamma \quad \Gamma \vdash b \dashv \Gamma}
$$

(Command Language Typing) 

$$
\begin{array}{l} \text { CMD - TY - USE - KEEP } \\ \vdash \Gamma_ {1} \quad \Gamma_ {1} (x) = k C L \\ k \neq \text { iso } \quad k \neq \text { var } \\ \Gamma_ {1} \vdash x: k C L \dashv \Gamma_ {1} \\ \text { CMD - TY - USE - DROP } \\ \vdash \Gamma_ {1} \quad \Gamma_ {1} = \Gamma [ x: t ] \\ \Gamma_ {2} = \Gamma [ x: \mathbf {u n d e f} ] \\ \Gamma_ {1} \vdash \mathbf {d r o p} x: t \dashv \Gamma_ {2} \\ \text { CMD - TY - LET } \\ \Gamma_ {1} \vdash b: t _ {1} \dashv \Gamma_ {2} \quad x \notin d o m (\Gamma_ {2}) \\ \Gamma_ {2}, x: t _ {1} \vdash e: t _ {2} \dashv \Gamma_ {3}, x: \_ \\ \Gamma_ {1} \vdash \mathbf {l e t} x = b \text {   in   } e: t _ {2} \dashv \Gamma_ {3} \\ \end{array}
$$

$$
\text { CMD - TY - TYPETEST }
$$

$$
\begin{array}{l} \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime} \quad \text { classtype } (t) \\ \Gamma^ {\prime}, y: t \vdash e _ {1}: t _ {1} \dashv \Gamma_ {1}, y: \_ \qquad \Gamma^ {\prime}, y: t ^ {\prime} \vdash e _ {2}: t _ {2} \dashv \Gamma_ {2}, y: \_ \\ \Gamma \vdash \text { if   } \text { typetest } (u s e, t) \{y = > e _ {1} \} \{y = > e _ {2} \}: t _ {1} \mid t _ {2} \dashv \Gamma_ {1} \mid \Gamma_ {2} \\ \end{array}
$$

$$
\text { CMD - TY - DEREF - FIELD }
$$

$$
\Gamma (x) = k C L
$$

$$
\mathbf {f t y p e} (C L, f) = t
$$

$$
\vdash k \odot t
$$

$$
\Gamma \vdash * x. f: (k \odot t) \dashv \Gamma
$$

$$
\text { CMD - TY - DEREF - VAR }
$$

$$
\Gamma (x) = \operatorname{var} \operatorname{Cell} [ t ] \qquad \vdash \operatorname{var} \odot t
$$

$$
\Gamma \vdash * x: (\operatorname{var} \odot t) \dashv \Gamma
$$

$$
\text { CMD - TY - ASSIGN }
$$

$$
\Gamma_ {1} \vdash u s e: t \dashv \Gamma_ {2} \quad \Gamma_ {2} (x) = k C L
$$

$$
\mathbf {f t y p e} (C L, f) = t \quad k \in \{\text { mut }, \text { tmp } \}
$$

$$
\Gamma_ {1} \vdash x. f := u s e: t \dashv \Gamma_ {2}
$$

$$
\text { CMD - TY - ASSIGN - VAR }
$$

$$
\Gamma_ {1} \vdash u s e: t _ {1} \dashv \Gamma_ {2} [ x: \operatorname{varCell} [ t _ {2} ] ]
$$

$$
\Gamma_ {1} \vdash x := u s e: t _ {2} \dashv \Gamma_ {2} [ x: \operatorname{varCell} [ t _ {1} ] ]
$$

$$
\text { CMD - TY - CREATE - VAR }
$$

$$
\Gamma_ {1} \vdash u s e: t \dashv \Gamma_ {2}
$$

$$
\Gamma_ {1} \vdash \text {   var   use   }: \text {   var   Cell } [ t ] \dashv \Gamma_ {2}
$$

$$
\text { CMD - TY - CALL }
$$

$$
\mathbf {f n c t y p e} (f n c) = \left(t _ {1}, \dots , t _ {n}\right)\rightarrow t
$$

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1}
$$

$$
\Gamma_ {1} \vdash f n c (u s e _ {1}, \dots , u s e _ {n}): t \dashv \Gamma_ {n + 1}
$$

$$
\text { C   M   D   -   T   Y   -   N   E   W }
$$

$$
\vdash \Gamma_ {1} \quad \vdash k C \quad k \in \{\text { mut }, \text { tmp }, \text { iso } \}
$$

$$
\mathbf {f t y p e s} (C) = f _ {1}: t _ {1}, \dots , f _ {n}: t _ {n} \quad \forall i \in [ 1, n ]. (\Gamma_ {i} \vdash u s e _ {i}: t _ {i} ^ {\prime} \dashv \Gamma_ {i + 1} \wedge t _ {i} ^ {\prime} <  : t _ {i})
$$

$$
k = \text { iso } \Rightarrow \forall i \in [ 1, n ]. \text { cap } (\{\text { iso }, \text { imm } \}, t _ {i} ^ {\prime})
$$

$$
\Gamma_ {1} \vdash \mathbf {n e w} k C (u s e _ {1}, \dots , u s e _ {n}): k C \dashv \Gamma_ {n + 1}
$$

cmd-ty-enter 

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1} \quad \quad \Gamma_ {n + 1} (x) = k C L
$$

$$
\operatorname{open} (k) \quad \text { f   t   y   p   e } (C L, f) = t \quad \operatorname{cap} (\operatorname{iso}, t)
$$

$$
\Gamma^ {\prime} = y _ {1}: t _ {1} ^ {\prime}, \dots , y _ {n}: t _ {n} ^ {\prime} \text {where} t _ {i} ^ {\prime} = \left\{ \begin{array}{l l} t _ {i} & \text {if cap(iso, t_{i})} \\ \text {paused} \odot t _ {i} & \text {otherwise} \end{array} \right.
$$

$$
t ^ {\prime} = \text { make\_mut } (t) \quad \Gamma^ {\prime}, z: \text { tmp   Cell } [ t ^ {\prime} ] \vdash e: t ^ {\prime \prime} \dashv \Gamma^ {\prime \prime}, z: \text { tmp   Cell } [ t ^ {\prime} ] \quad \text { cap } (\{\text { iso }, \text { imm } \}, t ^ {\prime \prime})
$$

$$
\Gamma_ {1} \vdash \text { enter } x. f [ y _ {1} = u s e _ {1}, \dots , y _ {n} = u s e _ {n} ] \{z = > e \}: t ^ {\prime \prime} \dashv \Gamma_ {n + 1}
$$

cmd-ty-enter-var 

$$
\forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1} \quad \Gamma [ x: \operatorname{varCell} [ t ] ] = \Gamma_ {n + 1} \quad \operatorname{cap} (\text { iso }, t)
$$

$$
\Gamma^ {\prime} = y _ {1}: t _ {1} ^ {\prime}, \dots , y _ {n}: t _ {n} ^ {\prime} \text {where} t _ {i} ^ {\prime} = \left\{ \begin{array}{l l} t _ {i} & \text {if cap(iso, t_{i})} \\ \text {paused} \odot t _ {i} & \text {otherwise} \end{array} \right.
$$

$$
\Gamma^ {\prime}, z: \text { var   Cell } [ m a k e \_ m u t (t) ] \vdash e: t ^ {\prime \prime} \dashv \Gamma^ {\prime \prime}, z: \text { var   Cell } [ t ^ {\prime} ]
$$

$$
\operatorname{cap} (\text { mut }, t ^ {\prime}) \quad \operatorname{cap} (\{\text { iso }, \text { imm } \}, t ^ {\prime \prime})
$$

$$
\Gamma_ {1} \vdash \text { enter } x [ y _ {1} = u s e _ {1}, \dots , y _ {n} = u s e _ {n} ] \{z = > e \}: t ^ {\prime \prime} \dashv \Gamma [ x: \text { var   Cell } [ m a k e \_ i s o (t ^ {\prime}) ] ]
$$

cmd-ty-freeze 

$$
\Gamma_ {1} \vdash u s e: \text { iso } C L \dashv \Gamma_ {2}
$$

$$
\Gamma_ {1} \vdash \text { freeze   use }: \text { imm } C L \dashv \Gamma_ {2}
$$

cmd-ty-merge 

$$
\Gamma_ {1} \vdash u s e: \text { iso } C L \dashv \Gamma_ {2}
$$

$$
\Gamma_ {1} \vdash \text { merge   use }: \text { mut } C L \dashv \Gamma_ {2}
$$

cmd-ty-split 

$$
\frac {\Gamma_ {1} [ x : t ] \vdash b : t _ {1} \dashv \Gamma_ {2} \qquad \Gamma_ {1} [ x : t ^ {\prime} ] \vdash b : t _ {1} ^ {\prime} \dashv \Gamma_ {2} ^ {\prime}}{\Gamma_ {1} [ x : t | t ^ {\prime} ] \vdash b : t _ {1} | t _ {1} ^ {\prime} \dashv \Gamma_ {2} | \Gamma_ {2} ^ {\prime}}
$$

cmd-ty-sub 

$$
\frac {\Gamma_ {1} \vdash b : t _ {1} \dashv \Gamma_ {2} \qquad t _ {1} <   : t _ {2}}{\Gamma_ {1} \vdash b : t _ {2} \dashv \Gamma_ {2}}
$$

$$
\overline {{\Gamma}} \vdash d e \dashv \Gamma \quad \overline {{\Gamma}} \vdash d b \dashv \Gamma
$$

(Command Language Dynamic Typing) 

cmd-dyn-ty-expr 

$$
\frac {\Gamma \vdash e : t \dashv \Gamma^ {\prime}}{\varepsilon : : \Gamma \vdash e : t \dashv \Gamma^ {\prime}}
$$

cmd-dyn-ty-let 

$$
\frac {\overline {{\Gamma}} : : \Gamma \vdash d b : t \dashv \Gamma^ {\prime} \qquad \Gamma^ {\prime} , x : t \vdash e : t ^ {\prime} \dashv \Gamma^ {\prime \prime} , x : \_}{\overline {{\Gamma}} : : \Gamma \vdash \mathbf {l e t} x = d b \textbf {i n} e : t ^ {\prime} \dashv \Gamma^ {\prime \prime}}
$$

cmd-dyn-ty-entered 

$$
\operatorname{open} (k) \quad \mathbf {f t y p e} (C L, f) = t \quad \operatorname{cap} (\text { iso }, t)
$$

$$
\overline {{\Gamma}}:: \Gamma_ {1} \vdash d e: t ^ {\prime} \dashv \Gamma_ {1} ^ {\prime} [ y: k ^ {\prime} \operatorname{Cell} [ t ^ {\prime \prime} ] ] \quad \text { cap } (\{\text { iso }, \text { imm } \}, t ^ {\prime})
$$

$$
k ^ {\prime} \in \{\text { tmp }, \text { var } \} \quad \text { cap } (\text { mut }, t ^ {\prime \prime})
$$

$$
k ^ {\prime \prime} C L ^ {\prime \prime} = \left\{ \begin{array}{l l} \text {var Cell} [ m a k e \_ i s o (t ^ {\prime \prime}) ] & \text {if k CL = var Cell[\_]} \\ k C L & \text {otherwise} \end{array} \right.
$$

$$
\overline {{\Gamma}}:: \Gamma_ {1} \underset {x. f} {\therefore} (\Gamma_ {2} [ x: k C L ]) \vdash \textbf {e n t e r e d} x. f y. \text { val } \{d e \}: t ^ {\prime} \dashv \Gamma_ {2} [ x: k ^ {\prime \prime} C L ^ {\prime \prime} ]
$$

cmd-dyn-ty-sub 

$$
\frac {\overline {{\Gamma}} \vdash b : t _ {1} \dashv \Gamma_ {2} \qquad t _ {1} <   : t _ {2}}{\overline {{\Gamma}} \vdash b : t _ {2} \dashv \Gamma_ {2}}
$$

cmd-dyn-ty-split 

$$
\frac {\overline {{\Gamma}} [ x : t ] \vdash b : t _ {1} \dashv \Gamma_ {2} \qquad \overline {{\Gamma}} [ x : t ^ {\prime} ] \vdash b : t _ {1} ^ {\prime} \dashv \Gamma_ {2} ^ {\prime}}{\overline {{\Gamma}} [ x : t | t ^ {\prime} ] \vdash b : t _ {1} | t _ {1} ^ {\prime} \dashv \Gamma_ {2} | \Gamma_ {2} ^ {\prime}}
$$

cmd-dyn-ty-failure 

$$
\overline {{\Gamma}}:: \Gamma \vdash \text {   Failure   }: t \dashv \Gamma
$$

⊢ ?? 

(Well-formedness of types) 

ty-class 

$$
k \neq \text { var } \quad \text {   class   } C \{f d e c l s \} \in p r o g
$$

$$
\forall f: t \in f d e c l s. \forall k ^ {\prime} C \in t. (k ^ {\prime} = \mathrm{isoV} \vdash k \odot k ^ {\prime})
$$

$$
\vdash k C
$$

ty-cell 

$$
\vdash t
$$

$$
\vdash k \operatorname{Cell} [ t ]
$$

ty-disj 

$$
\vdash t _ {1} \qquad \vdash t _ {2}
$$

$$
\vdash t _ {1} \mid t _ {2}
$$

$$
\boxed {t <  : t}
$$

(Subtyping) 

sub-refl 

$$
\frac {\vdash t}{t <   : t}
$$

sub-disj-right-1 

$$
\frac {t _ {1} <   : t _ {2}}{t _ {1} <   : t _ {2} \mid t _ {3}}
$$

sub-disj-right-2 

$$
\frac {t _ {1} <   : t _ {3}}{t _ {1} <   : t _ {2} \mid t _ {3}}
$$

sub-disj-left 

$$
\frac {t _ {1} <   : t _ {3} \qquad t _ {2} <   : t _ {3}}{t _ {1} \mid t _ {2} <   : t _ {3}}
$$

sub-cell 

$$
\frac {t _ {1} <   : t _ {2} \qquad t _ {2} <   : t _ {1}}{k \operatorname{Cell} [ t _ {1} ] <   : k \operatorname{Cell} [ t _ {2} ]}
$$

$$
\boxed {\overline {{\Gamma}} \vdash \{d e \}}
$$

(Well-formedness of dynamic configuration) 

$$
\frac {\exists \Gamma^ {\prime} , t . \overline {{\Gamma}} \vdash d e : t \dashv \Gamma^ {\prime}}{\overline {{\Gamma}} \vdash \{d e \}}
$$

$$
\boxed {\vdash \Gamma}
$$

(Well-formedness of typing contexts) 

wf-ctx-cons 

$$
\frac {x \notin d o m (\Gamma) \qquad \vdash t \qquad \vdash \Gamma}{\vdash \Gamma , x : t}
$$

wf-ctx-cons-undef 

$$
\frac {x \notin d o m (\Gamma) \qquad \vdash \Gamma}{\vdash \Gamma , x : \mathbf {u n d e f}}
$$

$$
\begin{array}{l} \text {WF - CTX - EMPTY} \\ \underline {{\quad}} \\ \vdash \varepsilon \end{array}
$$

$$
\boxed {\vdash \overline {{\Gamma}}}
$$

(Well-formedness of dynamic typing contexts) 

wf-dyn-ctx-cons 

$$
\vdash \Gamma_ {1} \qquad \vdash \Gamma_ {2} \qquad \Gamma_ {2} (x) = t \qquad \vdash \overline {{\Gamma}}
$$

$$
\frac{\forall k C\in t.  (\mathbf{f}\mathbf{t}\mathbf{y}\mathbf{p}\mathbf{e}\left(C,f\right) = t^{\prime}\wedge cap(iso,t^{\prime}))}{\vdash \Gamma_{1}:: \Gamma_{2}:: \overline{\Gamma}}
$$

wf-dyn-ctx-empty 

$$
\frac{\vdash\Gamma}{\vdash\Gamma\:: \: \varepsilon_{x.f}}
$$

# F PROOFS

# F.1 Theorem: Progress of Command Language

$$
\overline {{{\Gamma}}} \vdash \{d e \} \implies
$$

$$
d e = u s e \vee d e = \text { Failure } \vee \exists E f f, d e ^ {\prime}. d e \xrightarrow {E f f} d e ^ {\prime}
$$

F.1.1 Proof Sketch. The proof of progress is very simple since there are very few rules that have premises that can be false: function calls must refer to existing functions and objects can only be constructed with capability mut, tmp or iso. All other rules are always applicable when the expression has the right shape. 

F.1.2 Proof of Theorem. We begin by inversion of $\overline { { \Gamma } } \vdash \{ d e \}$ and get $\overline { { \Gamma } } \vdash d e : t \vdash 1 \Gamma ^ { \prime }$ . We proceed by induction over the dynamic typing judgment. 

Case 1 CMD -DYN -T Y-FAILURE:, ???? = Failure 

Proof holds trivially 

Case 2 CMD -DYN -T Y-EXPR:, ???? = ?? 

$$
\mathrm{A1} \overline {{\Gamma}} = \Gamma
$$

We proceed by induction over the static typing judgment $\Gamma \vdash d e : t \vdash \Gamma ^ { \prime }$ : 

Case 2.a CMD -T Y-USE -KEEP:, ???? = ?? 

Proof holds trivially. 

Case 2.b CMD -T Y-USE -DROP:, ???? = drop ?? 

Proof holds trivially. 

Case 2.c CMD -T Y-T YPETEST:, ???? = if typetest(use, ??1){??=>??1}{??=>??2} 

We pick any ?? ????. 

– $\mathrm { I f } k C L < : t ,$ , the configuration steps by CMD -IF -T YPETEST-TRUE. 

– If ?? ???? ≮: ??, the configuration steps by CMD -IF -T YPETEST-FALSE. 

Case 2.d CMD -T Y-LET: , ???? = let ?? = ?? in ?? 

By CMD -T Y-LET we have 

A1 $\Gamma \vdash b : t ^ { \prime } \dashv \Gamma ^ { \prime \prime }$ 

We proceed by cases on the shape of ??. 

$- \mathrm { ~ I f ~ } b \ = \ f n c ( u s e _ { 1 } , . . . , u s e _ { n } )$ , then by the inversion lemma with A1 we get fnctype $( f n c ) = ( t _ { 1 } , . . . , t _ { n } )  t ^ { \prime \prime }$ , and thus by assumptions about the function table fnclookup $( f n c ) = ( x _ { 1 } , . . . , x _ { n } )  e _ { b o d y }$ . The configuration steps by CMD -CALL. 

– If ?? = new $k C ( u s e _ { 1 } , . . . , u s e _ { n } )$ , then by the inversion lemma with A1 we get ?? ∈ {mut, tmp, iso}. The configuration steps by CMD -LET-EFF 

– $\mathrm { I f } \ : b = \tt { e n t e r } . . . ,$ the configuration steps by CMD -EN TER -FIELD, CMD -EN TER -VAR or CMD -EN TER -FAIL 

– For all remaining cases, the configuration steps by CMD -LET-EFF. 

Case 3 CMD -DYN -T Y-LET, ???? = let ?? = ???? in ??: 

A1 $\overline { { \Gamma } } = \overline { { \Gamma } } ^ { \prime } : : \Gamma$ 

We proceed by cases on whether ???? is in the syntactic category of ?? or not. 

– If it is, the proof collapses to case Case 2.d , since every non-dynamic typing derivation needs to use CMD -DYN -T Y-EXPR, which requires an empty Γ. 

– If it is not, we have either $d b = d e ^ { \prime }$ or $d b = \mathsf { e n t e r }$ ed $y . f$ ?? .val{????′}. We proceed by cases. 

Case 3.a $d b = d e ^ { \prime } $ : 

– By CMD -T Y-LET we have $\overline { { \Gamma } } \vdash d e ^ { \prime } : t ^ { \prime } \dashv { \Gamma } ^ { \prime \prime }$ . 

– By the induction hypothesis we have $d e ^ { \prime } \ = \ u s e ,$ or $d e ^ { \prime } \ = \ \mathbf { F a i l u r e }$ or $\exists E f f , d e ^ { \prime \prime } . d e ^ { \prime } \xrightarrow { E f f } d e ^ { \prime \prime }$ . We proceed by cases. 

– If ????′ = use, configuration steps by CMD -EFF. 

– If ????′ = Failure, configuration steps by CMD -EC -FAIL. 

$- \ \mathrm { I f } \ d e ^ { \prime } \ { \xrightarrow { E f f } } \ d e ^ { \prime \prime } ,$ , configuration steps by CMD -EC. 

Case 3.b ???? = entered ??.?? ??.val{????′}: 

B1 $\overline { { { \Gamma } } } = \overline { { { \Gamma } } } ^ { \prime \prime } : : \Gamma ^ { \prime } : : \Gamma$ 

– By CMD -T Y-LET we have Γ ⊢ entered $x . f y . \mathrm { v a l } \{ d e ^ { \prime } \} : t ^ { \prime } + \Gamma ^ { \prime \prime }$ . 

– By the inversion lemma, we have $\overline { { \Gamma } } ^ { \prime \prime } : \Gamma ^ { \prime } \vdash d e ^ { \prime } : t ^ { \prime } \dashv \Gamma _ { 1 } ^ { \prime }$ 

– By the induction hypothesis we have $d e ^ { \prime } \ = \ u s e ,$ or $d e ^ { \prime } \ = \ \mathbf { F a i l u r e \ o r }$ ∃Eff , $d e ^ { \prime \prime } . d e ^ { \prime } \xrightarrow { E f f } d e ^ { \prime \prime }$ . We proceed by cases. 

– If ????′ = use, configuration steps by CMD -EC with CMD -EXI T. 

– If ????′ = Failure, configuration steps by CMD -EC with CMD -EC -FAIL. 

$- \ \mathrm { I f } \ d e ^ { \prime } \ { \xrightarrow { E f f } } \ d e ^ { \prime \prime }$ , configuration steps by CMD -EC with CMD -EC 

# F.2 Theorem: Preservation of Command Language

$$
\begin{array}{c} \overline {{\Gamma}} \vdash \{d e \} \wedge \{d e \} \xrightarrow {E f f} \{d e ^ {\prime} \} \Longrightarrow \\ \exists \overline {{\Gamma}} ^ {\prime}. \overline {{\Gamma}} ^ {\prime} \vdash \{d e ^ {\prime} \} \wedge \overline {{\Gamma}} \vdash E f f + \overline {{\Gamma}} ^ {\prime} \end{array}
$$

F.2.1 Proof Sketch. The proof of preservation ensures that stepping preserves well-formedness and that it produces a well-formed effect. The former is straightforward since the only changes we make to the configuration is alpha conversion of subexpressions, introduction of (well-formed) function bodies, and introduction of entered blocks from (well-formed) enter blocks. The latter comes from lifting the rules for expression typing to the rules for well-formed effects, via the inversion lemma. 

F.2.2 Proof of Theorem. We state an alpha conversion property, the observation that we can change the name of a variable without affecting well-formedness of an expression: 

$$
\begin{array}{l} \Gamma [ x: t _ {1} ] \vdash e: t \dashv \Gamma^ {\prime} [ x: t _ {2} ] \wedge y f r e s h \implies \\ \Gamma [ y: t _ {1} ] \vdash e [ x \mapsto y ]: t \dashv \Gamma^ {\prime} [ y: t _ {2} ] \\ \end{array}
$$

$$
\Gamma [ x: t _ {1} ] \vdash e: t \dashv \Gamma^ {\prime} [ x: \mathbf {u n d e f} ] \wedge y f r e s h \implies
$$

$$
\Gamma [ y: t _ {1} ] \vdash e [ x \mapsto y ]: t \dashv \Gamma^ {\prime} [ y: \mathbf {u n d e f} ]
$$

We also note that the type system supports weakening of the typing context as long as the bound variables in the expression are not in the domain of the added context. 

$$
\Gamma_ {1} \vdash e: t \dashv \Gamma_ {2} \wedge \vdash \Gamma , \Gamma_ {1} \wedge \mathbf {B V} (e) \cap \mathbf {d o m} (\Gamma) = \emptyset \implies
$$

$$
\Gamma , \Gamma_ {1} \vdash e: t \dashv \Gamma , \Gamma_ {2}
$$

We start the proof of preservation by inversion of well-formedness of the configuration and get Γ ⊢ ???? : ?? ⊣ Γ′. We proceed by induction over the dynamic typing judgment. 

Case 1 CMD -DYN -T Y-FAILURE:, ???? = Failure 

Proof holds vacuously as the configuration does not step. 

Case 2 CMD -DYN -T Y-EXPR:, ???? = ?? 

$$
\mathrm{G1} \overline {{\Gamma}} = \Gamma
$$

We proceed by induction over the static typing judgment Γ ⊢ ???? : ?? ⊣ Γ′: 

Case 2.a CMD -T Y-USE -KEEP:, ???? = ?? 

Proof holds vacuously as the configuration does not step. 

Case 2.b CMD -T Y-USE -DROP:, ???? = drop ?? 

Proof holds vacuously as the configuration does not step. 

Case 2.c CMD -T Y-T YPETEST:, ???? = if typetest(use, ??′){??=>??1}{??=>??2} 

By CMD -T Y-T YPETEST we have 

A1 Γ ⊢ use : ?? ′′ ⊣ Γ′′ 

A2 Γ′′, ?? : ?? ′ ⊢ ??1 : ??1 ⊣ Γ1, ?? : _ 

A3 Γ′′, ?? : ?? ′′ ⊢ ??2 : ??2 ⊣ Γ2, ?? : _ 

There are two rules that step the configuration. 

– CMD -IF -T YPETEST-TRUE 

B1 Eff = cast(??′, use, ?? ????) 

B2 ?? ???? <: ?? ′ 

B3 ??′ fresh 

– By the alpha conversion property with A2 , we have Γ′′, ??′ : ?? ′ ⊢ ??1 [?? ↦→ 

$$
\left. y ^ {\prime} \right]: t _ {1} \dashv \Gamma_ {1}, y ^ {\prime}: \_.
$$

– The resulting configuration is well-formed by CMD -DYN -T Y-EXPR. 

– The effect is well-formed under Γ By WF -EFF -CAST with A1 and B2 . 

– CMD -IF -T YPETEST-FALSE 

Similar to the above. 

Case 2.d CMD -T Y-LET: , ???? = let ?? = ?? in ?? 

By CMD -T Y-LET we have 

A1 Γ ⊢ ?? : ?? ′ ⊣ Γ′′ 

A2 Γ′′, ?? : ?? ′ ⊢ ?? : ?? ⊣ Γ′, ?? : _ 

There are seven rules that step the configuration. 

– CMD -LET-EFF 

B1 ?? ′ fresh 

B2 effect(??′, ??) = Eff 

– By the alpha conversion property with A2 , we have Γ′′, ??′ : ?? ′ ⊢ ?? [?? ↦→ ?? ′] : ?? ⊣ Γ′′, ?? ′ : _, and the resulting configuration is well-formed by CMD -DYN -T Y-EXPR. 

– Depending on the shape of ?? there are eleven cases of effect(??′, ??) = Eff : 

– effect(?? ′, ∗y.f) = load(??, ??.?? ) 

The effect is well-formed by WF -EFF -LOAD, by the inversion lemma with A1 , followed by induction over the shape of ?? ′. 

– effect(?? ′, ∗y) = load(??, ??.?????? ) 

Similar to the previous case. 

– effect(?? ′, = y.f c:use) = swap(??, ??.?? , use) 

The effect is well-formed by WF -EFF -SWAP -CLASS, by the inversion lemma with A1 , followed by induction over the shape of ?? ′. 

– effect(?? ′, = y c:use) = swap(??, ??.??????, use) 

Similar to the previous case, but using WF -EFF -SWAP -VAR. 

– effect(?? ′, new mut ?? (use)) = halloc(??, mut, #??, use) 

The effect is well-formed by WF -EFF -HALLOC -MU T, by the inversion lemma with A1 . For the typing of each use, we use CMD -T Y-SUB since the inversion lemma gives us that the arguments are subtypes of the field types. 

– effect(?? ′, new iso ?? (use)) = halloc(??, iso, #??, use) 

Similar to the previous case, but using WF -EFF -HALLOC -ISO and without the need for CMD -T Y-SUB (note that all the arguments are iso or imm). 

– effect(?? ′, new tmp ?? (use)) = salloc(??, tmp, #??, use) 

Similar to the case for mut allocation, but using WF -EFF -SALLOC. 

– effect(?? ′, var use) = salloc(??, var, #Cell, use) 

Similar to the previous case. 

– effect(??′, freeze use) = freeze(??, use) 

The effect is well-formed by WF -EFF -FREEZE, by the inversion lemma with A1 . 

– effect(??′, merge use) = merge(??, use) 

The effect is well-formed by WF -EFF -MERGE, by the inversion lemma with A1 . 

– effect(?? ′, use) = bind(?? = use) 

The effect is well-formed by WF -EFF -BIND with A1 . 

– CMD -EN TER -FIELD, ?? = enter ??.?? [?? = use]{?? => ??′} 

B1 ?? ′ fresh 

B2 ??′ , ..., ??′?? fresh 

B3 $E f f = e n t e r ( w ^ { \prime } , \mathrm { t m p } , y . f , z _ { 1 } ^ { \prime } = u s e _ { 1 } , . . . , z _ { n } ^ { \prime } = u s e _ { n } )$ 

B4 ???? ′ = let ?? = entered ?? . ?? ?? ′ .val{?? ′ [?? ↦→ ?? ′, ??1 ↦→ ??′1, ..., ???? ↦→ ??′1]} 

By the inversion lemma with A1 we get 

B5 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } + \Gamma _ { i + 1 }$ 

B6 $\Gamma _ { n + 1 } ( y ) = t _ { y }$ 

B7 cap({mut, tmp, var, paused}, $t _ { y } )$ 

B8 $\mathbf { f r e s u l t } ( t _ { y } , f ) ) = t _ { f }$ 

B9 cap(iso, ???? ) 

B10 $\Gamma ^ { \prime } = y _ { 1 } : t _ { 1 } ^ { \prime } , . . . , y _ { n } : t _ { n } ^ { \prime }$ where $t _ { i } ^ { \prime } = \left\{ { { t _ { i } } \atop { \mathrm { p a u s e d } \odot t _ { i } } } \right.$ = if cap(iso, ???? ) otherwise 

B11 $t _ { f } ^ { \prime } = m a k e \_ m u t ( t _ { f } )$ 

B12 $\mathsf { c a p } ( \{ \mathsf { i s o } , \mathsf { i m m } \} , t ^ { \prime } )$ 

B13 $t ^ { \prime } < : t$ 

B14 $\Gamma ^ { \prime } , z : \mathrm { t m p } \mathsf { c e l 1 } [ t _ { f } ^ { \prime } ] \vdash e : t ^ { \prime } + \Gamma ^ { \prime } , z : \mathrm { t m p } \mathsf { c e l 1 } [ t _ { f } ^ { \prime } ]$ 

– entered $y . f \ w ^ { \prime } . \mathrm { v a l } \{ e ^ { \prime } [ w \mapsto w ^ { \prime } , z _ { 1 } \mapsto z _ { 1 } ^ { \prime } , . . . , z _ { n } \ \mapsto \ z _ { 1 } ^ { \prime } ]$ is well-formed by induction over the shape of $t _ { y }$ . In the inductive case we use CMD -DYN -T Y-SPLI T. In the base case we use the alpha conversion property and CMD -DYN - T Y-EN TERED with B7 , B8 , B9 , B14 and B12 . 

– ????′ is well-formed by CMD -DYN -T Y-SUB with B13 and CMD -DYN -T Y-LET with the above and A2 . 

– The effect is well-formed by induction over the shape of $t _ { y }$ . In the inductive case we use WF -EFF -SPLI T. In the base case we use WF -EFF -EN TER with B5 , B6 , B7 and B9 , with resulting dynamic context $( \Gamma ^ { \prime } , z : \mathrm { t m p } \mathsf { C e l } 1 [ t _ { f } ^ { \prime } ] ) : \Gamma _ { n + 1 }$ 

– CMD -EN TER -VAR, ?? = enter $y \ [ { \overline { { z = u s e } } } ] \{ w = > e ^ { \prime } \}$ 

Similar to the previous case. 

– CMD -EN TER -FIELD -FAIL 

B1 $E f f = b a d e n t e r ( y . f )$ 

B2 $d e ^ { \prime } = \mathbf { F a i l u r e }$ 

By the inversion lemma with A1 we get 

B3 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } + \Gamma _ { i + 1 }$ 

B4 $\Gamma _ { n + 1 } ( y ) = t _ { y }$ 

B5 ${ \mathrm { c a p } } ( \{ \mathrm { m u t , t m p , v a r , p a u s e d } \} , t _ { y } )$ 

B6 $\mathbf { f r e s u l t } ( t _ { y } , f ) ) = t _ { f }$ 

B7 $\mathsf { c a p } ( \mathsf { i s o } , t _ { f } )$ 

– The resulting configuration {Failure} is always well-formed. 

– The effect is well-formed by induction over the shape of $t _ { y } .$ . In the inductive case we use WF -EFF -SPLI T. In the base case we use WF -EFF -BADEN TER with B4 , B5 , B6 and B7 . 

– CMD -EN TER -VAR -FAIL 

Similar to the previous case. 

– CMD -CALL, ?? = fnc(use1, ..., use??) 

B1 fnclookup $( f n c ) = ( x _ { 1 } , . . . , x _ { n } )  e _ { b o d y }$ 

B2 $x _ { 1 } ^ { \prime } , . . . , x _ { n } ^ { \prime } f r e s h$ 

B3 $\overline { { x } } = x _ { 1 } , . . . , x _ { n }$ 

B4 $\overline { { x } } ^ { \prime } = x _ { 1 } ^ { \prime } , . . . , x _ { n } ^ { \prime }$ 

B5 $E f f = \bar { b } i n d ( x _ { 1 } ^ { \prime } = u s e _ { 1 } , . . . , x _ { n } ^ { \prime } = u s e _ { n } )$ 

By the inversion lemma with A1 we get 

B6 $t ^ { \prime \prime } < : t ^ { \prime }$ 

B7 fnctype(fnc) = (??1, ..., ????) → ?? ′′ 

B8 $\Gamma = \Gamma _ { 1 }$ 

B9 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } + \Gamma _ { i + 1 }$ 

B10 $\Gamma ^ { \prime \prime } = \Gamma _ { n + 1 }$ 

– By assumptions about the function table we have $x _ { 1 } : t _ { 1 } , . . . , x _ { n } : n _ { n } \vdash e :$ ?? ′′ ⊣ Γ?????? . $t ^ { \prime \prime }  \Gamma _ { r e s } .$ 

– By the alpha conversion property we have $x _ { 1 } ^ { \prime } : t _ { 1 } , . . . , x _ { n } ^ { \prime } : n _ { n } \vdash e [ \overline { { { x } } } \mapsto \overline { { { x ^ { \prime } } } } ]$ : $t ^ { \prime \prime }  \Gamma _ { r e s }$ . 

– By the weakening property (we assume function lookup dynamically renames all variables to avoid clashes) we have $\Gamma ^ { \prime \prime } , x _ { 1 } ^ { \prime } : t _ { 1 } , . . . , x _ { n } ^ { \prime } : n _ { n } \vdash$ $e [ \overline { { { x } } } \mapsto \overline { { { x ^ { \prime } } } } ] : t ^ { \prime \prime } \not { q } \Gamma ^ { \prime \prime } , \Gamma _ { r e s }$ , and the resulting configuration is well-formed by CMD -DYN -T Y-EXPR, with CMD -T Y-SUB with B6 and CMD -T Y-LET. 

– The effect is well-formed by WF -EFF -BIND with B9 . 

– CMD -EC 

Preservation holds by CMD -DYN -T Y-LET with the induction hypothesis and A2 

Case 3 CMD -DYN -T Y-LET, ???? = let ?? = ???? in ??: 

A1 $\overline { { \Gamma } } = \overline { { \Gamma } } ^ { \prime } : : \Gamma$ 

A2 $\overline { { \Gamma } } ^ { \prime } : : \Gamma \vdash d b : t ^ { \prime } \ l + \Gamma ^ { \prime \prime }$ 

A3 $\Gamma ^ { \prime \prime } , x : t ^ { \prime } \vdash e : t + \Gamma ^ { \prime } , x : \_$ 

We proceed by cases on whether ???? is in the syntactic category of ?? or not. 

– If it is, the proof collapses to case Case 2.d , since every non-dynamic typing derivation needs to use CMD -DYN -T Y-EXPR, which requires a singleton ${ \overline { { \Gamma } } } .$ . 

– If it is not, we have either $d b = d e ^ { \prime \prime }$ , where $d e ^ { \prime \prime }$ is not in??, or ???? = entered??.?? ?? .val $\{ d e ^ { \prime \prime } \}$ We proceed by cases. 

Case 3.a $d b = d e ^ { \prime \prime } \colon$ 

Since $d e ^ { \prime \prime }$ is not in ??, there are only two dynamic rules that apply: 

– CMD -EC 

Preservation holds by CMD -DYN -T Y-LET with the induction hypothesis and A3 . 

– CMD -EC -FAIL 

Preservation holds since the resulting expression {Failure} and the empty effect are trivially well-formed. 

Case 3.b ???? = entered $y . f z . \mathrm { v a l } \{ d e ^ { \prime \prime } \}$ : 

By the inversion lemma with A2 we get 

B1 $\Gamma = \Gamma _ { 0 } [ y : t _ { y } ]$ 

B2 $\overline { { { \Gamma } } } = \overline { { { \Gamma } } } ^ { \prime \prime } \ : \because \ : \Gamma _ { 1 } : : \ : \Gamma$ ??.?? 

B3 $\overline { { { \Gamma } } } ^ { \prime } \vdash d e ^ { \prime \prime } : t ^ { \prime } \dashv { } \Gamma _ { 1 } ^ { \prime }$ 

B4 cap({mut, tmp, var, paused}, $t _ { y } )$ 

B5 fresult $( t _ { y } , f ) = t _ { f }$ 

B6 cap(iso, ?? ) 

B7 cap({iso, imm}, ?? ′) 

B8 $t ^ { \prime } < : t$ 

B9 $\Gamma _ { 1 } ^ { \prime } ( z ) = t _ { z }$ 

B10 fresult $\left( t _ { z } , \mathrm { v a l } \right) = t ^ { \prime \prime }$ 

B11 cap(mut, ?? ′′) 

B12 cap({tmp, var}, ???? ) 

B13 $\Gamma ^ { \prime } = \Gamma _ { 0 } [ y : t _ { y } ^ { \prime } ]$ 

$$
\text {B14} t _ {y} ^ {\prime} = \left\{ \begin{array}{l l} m a k e \_ c e l l (m a k e \_ i s o (t ^ {\prime \prime})) & \text {if} t _ {y} = m a k e \_ c e l l (\_) \\ t _ {y} & \text {otherwise} \end{array} \right.
$$

There are two rules that step the configuration: 

– CMD -EXI T 

B15 $d e ^ { \prime \prime } = u s e$ 

B16 ?? fresh 

B17 $d e ^ { \prime } = e [ x \mapsto x ^ { \prime } ]$ 

– The resulting configuration is well-formed by the alpha conversion property with $_ { \textrm { A 3 } }$ . 

– Since the $d e ^ { \prime \prime }$ is non-dynamic, we have $\overline { { \Gamma } } ^ { \prime \prime } = \varepsilon .$ 

– The effect is well-formed by induction over the shape of $t _ { z }$ . In the inductive cases we use WF -EFF -SPLI T. In the base case we use WF -EFF -EXI T with B3 , B7 , B11 , B6 , B12 and B14 . 

– CMD -EC 

Preservation holds by induction on the shape of $t _ { z } .$ . In the inductive cases we use WF -EFF -SPLI T. In the base case we use CMD -DYN -T Y-LET with CMD - DYN -T Y-EN TERED with the assumptions given by the inversion lemma. Note that the induction hypothesis only gives well-formedness for $E \mathcal { f }$ for $\overline { { \Gamma } } ^ { \prime }$ , but adding $\Gamma _ { 1 } ^ { \prime }$ to the right of $\overline { { \Gamma } } ^ { \prime }$ does not affect well-formedness of the effect since the well-formedness rules for effects looks at the dynamic typing context from the left. 

# F.3 Theorem: Progress of Region Semantics

$$
\vdash \langle \{d e \} \langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \wedge \{d e \} \xrightarrow {E f f} \{d e ^ {\prime} \} \implies
$$

$$
\exists E f f ^ {\prime}, \{d e \} \xrightarrow {E f f ^ {\prime}} \{d e ^ {\prime} \} \land \langle R S; H _ {o p}; H _ {c l}; H _ {f r} \rangle \xrightarrow {E f f ^ {\prime}} \langle R S ^ {\prime}; H _ {o p} ^ {\prime}; H _ {c l} ^ {\prime}; H _ {f r} ^ {\prime} \rangle
$$

F.3.1 Proof Sketch. The proof of progress ensures that if the command semantics produces a wellformed effect, there is an effect that steps both the command semantics and the region semantics. In practice the two effects will only be different for dynamic type tests, where the command semantics always allows both effects cast and nocast with any type argument, and when entering a region, where the command semantics always allows both the effects enter and badenter. The region semantics is always able to “select” an effect that works for both semantics. The rest of the proof is straightforward and comes down to the fact that looking up a well-defined name in a well-formed state always succeeds. 

F.3.2 Proof of Theorem. Whenever we have a well-formed use of a variable, the corresponding get in the region semantics produces a value of the corresponding type, and looking up that value in the region stack or heap produces an object of that type. We call this the well-formed lookup property and it follows from lemmas F.7, F.8 and F.9. We will sometimes use the capability_ok property to narrow where we are looking for an object. For example, we know that a mut reference points to an open heap region. Similarly, an update of an existing object via a store, stack_store or heap_store always succeeds. We call this property well-formed stores. 

We start the proof by inversion of TANDEM -WF and get $\overline { { \Gamma } } \vdash \{ d e \}$ . By preservation of the command language we get $\overline { { \Gamma } } \vdash E f f \ l + \overline { { \Gamma } } ^ { \prime }$ . We proceed by induction over the well-formed effect judgment. 

Case 1 WF -EFF -EPS:, $E f f = \epsilon$ 

The configuration steps trivially by REGION -EPS. 

Case 2 WF -EFF -LOAD:, $E f f = l o a d ( x , y . f )$ 

A1 ${ \overline { { \Gamma } } } = \Gamma : : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 $\Gamma ( y ) = k C L$ 

A4 $\mathbf { f } \mathbf { t y } \mathbf { p e } ( C L , f ) = t$ 

A5 ?? ≠ iso 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) : R S ^ { \prime } .$ 

– By well-formed lookup with A3 we get $F ( y ) = ( k , \iota )$ 

– Since only iso references can point to closed regions by capability_ok, by A5 and well-formed lookup we have cfg_load $( ( r , S , F ) : \mathit { R S } ^ { \prime } , \mathit { H _ { o p } } { * } \mathit { H _ { f r } } , \iota ) = o [ f \mapsto v ]$ . 

– The configuration steps with Eff by REGION -LOAD with $E f f$ . 

Case 3 WF -EFF -SWAP -CLASS:, $E f f = s w a p ( x , y . f ,$ use) 

A1 ${ \overline { { \Gamma } } } = \Gamma : : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 Γ ⊢ use : ?? ⊣ Γ′ 

A4 $\Gamma ^ { \prime } ( y ) = k C L$ 

A5 ftype(????, ?? ) = ?? 

A6 ?? ∈ {mut, tmp} 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) :  R S ^ { \prime } .$ 

– By well-formed lookup with A3 we get $\mathbf { g e t } ( F , u s e ) = ( v , F ^ { \prime } )$ with a well-formed $F ^ { \prime }$ under $\Gamma ^ { \prime } .$ . 

– By well-formed lookup with A4 we get $F ^ { \prime } ( y ) = ( k , \iota )$ . 

– We proceed by cases on A6 

Case 3.a ?? = mut: 

– By capability_ok ?? points into region ?? on the heap and we have $H _ { o p } =$ $( r , S ^ { \prime } )$ . 

– By well-formed lookup we have load $( S ^ { \prime } , \iota ) = o [ f \mapsto v ^ { \prime } ] .$ 

– By well-formed stores we have store $( S ^ { \prime } , \iota , o [ f \mapsto v ] ) = S ^ { \prime \prime } .$ . 

– The configuration steps by REGION -SWAP -HEAP with $E \# .$ 

Case 3.b ?? = tmp: 

– By capability_ok ?? points into $S$ on the region stack. 

– By well-formed lookup we have load $( S , \iota ) = o [ f \mapsto v ^ { \prime } ] .$ 

– By well-formed stores we have store $( S , \iota , o [ f \mapsto v ] ) = S ^ { \prime }$ . 

– The configuration steps by REGION -SWAP -TEMP with $E \# .$ 

Case 4 WF -EFF -SWAP -VAR:, Eff = swap(??, ??.val, use) 

Similar to Case 3 going into Case 3.b . 

Case 5 WF -EFF -HALLOC -MU T:, Eff = halloc(??, mut, #??, use1, ..., use??) 

A1 $\overline { { \Gamma } } = \Gamma _ { 1 } : \overline { { \Gamma } } _ { 2 }$ 

A2 ?? fresh 

A3 $\mathbf { f t y p e s } ( C ) = f _ { 1 } : t _ { 1 } , . . . , f _ { n } : t _ { n }$ 

A4 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } \dashv \Gamma _ { i + 1 }$ 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F _ { 1 } ) :  R S ^ { \prime }$ 

– By induction over ??, and by well-formed lookup we have $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) =$ $( v _ { i } , F _ { i + 1 } )$ . 

– By A3 and the assumptions of the class table we have fields $( C ) = f _ { 1 } , . . . , f _ { n } .$ 

– The configuration steps by REGION -ALLOC -HEAP -MU T with $E \mathcal { f } .$ . 

Case 6 WF -EFF -HALLOC -ISO:, $E f f = h a l l o c ( \boldsymbol { x } , \mathrm { i } \boldsymbol { s } _ { 0 } , \# C , u s e _ { 1 } , . . . , u s e _ { n } )$ 

Similar to Case 5 . 

Case 7 WF -EFF -SALLOC:, $E f f = s a l l o c ( \boldsymbol { x } , \boldsymbol { k } , \# C L , u s e _ { 1 } , . . . , u s e _ { n } )$ 

Similar to Case 5 , but the allocation happens on the region stack instead. 

Case 8 WF -EFF -FREEZE:, $E f f = f r e e z e ( x , u s e )$ 

A1 ${ \overline { { \Gamma } } } = \Gamma : : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 Γ ⊢ use : $t \to \Gamma ^ { \prime }$ 

A4 cap(iso, ?? ) 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) :  R S ^ { \prime } .$ . 

– By well-formed lookup with A3 we get $\mathbf { g e t } ( F , u s e ) = \left( ( k , \iota ) , F ^ { \prime } \right)$ . 

– By capability_ok an iso reference in a variable, like $\iota ,$ must point into a closed region $R ,$ so that $H _ { c l } = R _ { * } H _ { c l } ^ { \prime }$ . 

– By capability_ok, outgoing references from a closed region cannot go into an open region. This means that any regions reachable from ?? must also be closed, allowing us to write $H _ { c l }$ as $R * ( H * H _ { c l } ^ { \prime \prime } )$ where $H = { \mathsf { r e a c h a b l e } } _ { _ - }$ _regions $( R , ( H * H _ { c l } ^ { \prime \prime } ) * H _ { o p } )$ . 

– The configuration steps by REGION -FREEZE with $E \mathcal { f } .$ 

Case 9 WF -EFF -MERGE:, $E f f = m e r g e ( x , u s e )$ 

Similar to Case 8 . 

Case 10 W ${ \mathrm { F } } { \mathrm { - E F F - B I N D } } ; { \mathrm { , ~ } } E f f = b i n d ( x _ { 1 } = u s e _ { 1 } , . . . , x _ { n } = u s e _ { n } )$ 

A1 $\overline { { \Gamma } } = \Gamma _ { 1 } : \overline { { \Gamma } } _ { 2 }$ 

A2 $\forall i \in [ 1 , n ] . x _ { i }$ fresh 

A3 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } \dashv \Gamma _ { i + 1 }$ 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) : R S ^ { \prime }$ 

– By induction over ??, and by well-formed lookup we have $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) =$ $( v _ { i } , F _ { i + 1 } )$ . 

– The configuration steps by REGION -BIND with $E \mathcal { f } .$ 

Case 11 WF -EFF -CAST:, $E f f = c a s t ( x ,$ use, ?? ??) 

A1 ${ \overline { { \Gamma } } } = \Gamma : : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 Γ ⊢ use : ?? ⊣ Γ′ 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) :  R S ^ { \prime } .$ 

– By well-formed lookup with A3 we get $\mathbf { g e t } ( F , u s e ) = \left( ( k , \iota ) , F ^ { \prime } \right)$ with a well-formed $F ^ { \prime }$ under $\Gamma ^ { \prime }$ . 

– By well-formed lookup we get cfg_load $( ( r , S , F ^ { \prime } ) : \mathit { R S } ^ { \prime } , \mathit { H _ { o p ^ { \star } } H _ { c l ^ { \star } } H _ { f r } , \mathit { l } } ) = ( \# C ^ { \prime } , _ { - } )$ . 

– We proceed by cases on whether $k C ^ { \prime } < : t$ or not 

– If $k C ^ { \prime } < : t$ , the command configuration steps by CMD -IF -T YPETEST-TRUE with effect cast(??, use, ?? ??)′, and the region configuration steps by REGION -CAST with the same effect. 

– If $k C ^ { \prime } \ \notin : \ t ,$ the command configuration steps by CMD -IF -T YPETEST-FALSE with effect nocast(??, use, ?? ??′), and the region configuration steps by REGION -NOCAST with the same effect. 

Case 12 WF -EFF -NOCAST:, $E f f = n o c a s t ( x , u s e , k C )$ 

Similar to the previous case. 

Case 13 WF -EFF -EN TER:, $E f f = e n t e r ( w , k , y . f , x _ { 1 } = u s e _ { 1 } , . . . , x _ { n } = u s e _ { n } )$ 

A1 ${ \overline { { \Gamma } } } = \Gamma : : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 $\forall i \in \left[ 1 , n \right] . x _ { i } f r e s h$ 

A4 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } \dashv \Gamma _ { i + 1 }$ 

A5 $\Gamma _ { n + 1 } ( y ) = k ^ { \prime } C L$ 

A6 ??′ ∈ {mut, tmp, var, paused} 

A7 $\mathbf { f t y p e } ( C L , f ) ) = t$ 

A8 $\mathsf { c a p } ( \mathsf { i s o } , t )$ 

A9 $k \in \{ \mathrm { t m p , v a r } \}$ 

– By WF -RS -CONS with A1 we have $R S = ( r , S , F ) : R S ^ { \prime }$ 

– By induction over ??, and by well-formed lookup we have $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) =$ $( v _ { i } , F _ { i + 1 } )$ . 

– By well-formed lookup we have $F _ { n + 1 } ( y ) = ( k ^ { \prime } , \iota )$ 

– By capability_ok references which are open point to an open region on the stack or heap. By well-formed lookup and by A6 we have $\mathrm { c f g \_ l o a d } ( ( r , S , F ^ { \prime } ) : \ R S ^ { \prime } , H _ { o p } , \iota ) =$ $o [ f \mapsto ( \_ , \iota ^ { \prime } ) ]$ . 

– We proceed by cases on whether ??′ is in the domain of any closed region. 

– If it is, the configuration steps by REGION -EN TER -OK. 

– If it is not, the configuration steps by REGION -EN TER -FAIL. 

# Case 14 WF -EFF -BADEN TER:

Similar to the previous case, noting that any destructive reads will not have made ?? undefined, as this would not have been well-typed in the command semantics. 

# Case 15 WF -EFF -EXI T:

A1 ${ \overline { { \Gamma } } } = \Gamma ^ { \prime } : : \Gamma [ y : k C L ] : { \overline { { \Gamma } } } _ { 2 }$ 

A2 ?? fresh 

A3 ?? ∈ {mut, tmp, var, paused} 

A4 $\Gamma ^ { \prime } \vdash u s e : t \vdash 1 \Gamma ^ { \prime \prime }$ 

A5 $\Gamma ^ { \prime \prime } ( w ) = k ^ { \prime } C L ^ { \prime }$ 

A6 $\mathbf { f t y p e } ( C L ^ { \prime } , g ) ) = t ^ { \prime }$ 

– By WF -RS -CONS with A1 we have $R S = \left( r ^ { \prime } , S ^ { \prime } , F ^ { \prime } \right) : : \left( r , S , F \right) : : R S ^ { \prime } .$ . 

– By well-formed lookup with A4 we have get $( F ^ { \prime } , u s e ) = ( v , F ^ { \prime \prime } )$ . 

– By well-formed lookup with A5 we have $F ^ { \prime \prime } ( z ) = ( \_ , \iota ^ { \prime } )$ and load $( S ^ { \prime } , \iota ^ { \prime } ) = o ^ { \prime } [ f ^ { \prime } \mapsto$ $\left( \_ , \iota ^ { \prime \prime } \right) ]$ . 

– By well-formed lookup with A1 we have $F ( y ) = ( \_ , \iota )$ . 

– Since ?? is open by A3 , by capability_ok ?? points to the open heap or the region stack. We proceed by cases. 

– If ?? points to the stack we have stack_load $( r , S , F ) : \ : R S , \iota = o [ f \mapsto ( k , _ { - } ) ]$ , and by well-formed stores stack_store $( ( r , S , F ) : \mathit { R S , } \iota , o [ f \mapsto ( k , \iota ^ { \prime \prime } ) ] ) = ( r , S ^ { \prime \prime } , F ) : : \mathit { R S ^ { \prime } }$ . The configuration steps by REGION -EXI T-TEMP. 

– If ?? points to the heap we have heap_load $( H _ { o p } , \iota ) = o [ f \mapsto ( k , \lrcorner ) ]$ , and by well-formed stores that heap_store $( H _ { o p } , \imath , o [ f \mapsto ( k , \imath ^ { \prime \prime } ) ] ) = H _ { o p } ^ { \prime }$ . The configuration steps by REGION -EXI T-HEAP. 

# Case 16 WF -EFF -SPLI T:

A1 $\overline { { \Gamma } } = \overline { { \Gamma } } _ { 0 } [ x : t | t ^ { \prime } ]$ 

A2 $\overline { { \Gamma } } _ { 0 } \left[ \boldsymbol { x } : t \right] \vdash E f f \vdash \overline { { \Gamma } } _ { 1 }$ 

A3 $\overline { { \Gamma } } _ { 0 } [ x : t ^ { \prime } ] \vdash E f f \dashv \overline { { \Gamma } } _ { 1 }$ 

– By WF -VARS -CONS, the type $t _ { x }$ of the value of ?? must be a subtype of $t | t ^ { \prime } .$ 

– By the subtyping rules, we must have $t _ { x } < : t$ or $t _ { x } < : t ^ { \prime }$ . We proceed by cases. 

– Without loss of generality, we assume $t _ { x } < : t$ (the other case is symmetric). 

– We have $\overline { { \Gamma } } _ { 0 } [ x : t ] \vdash \langle R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

– Progress holds by the induction hypothesis. 

# F.4 Theorem: Preservation of Region Semantics

The region semantics has a property similar to preservation that we express like this. 

$$
\begin{array}{l} \overline {{\Gamma}}; \Delta ; \Psi \vdash r c f g \wedge \text {   capability\_ok } (r c f g) \wedge \text {   topology\_ok } (r c f g) \wedge r c f g \xrightarrow {E f f} r c f g ^ {\prime} \wedge \overline {{\Gamma}} \vdash E f f \dashv \overline {{\Gamma}} ^ {\prime} \implies \\ \exists \Delta^ {\prime}, \Psi^ {\prime}. \end{array}
$$

$$
\overline {{\Gamma}} ^ {\prime}; \Delta^ {\prime}; \Psi^ {\prime} \vdash r c f g ^ {\prime} \land \text { capability\_ok } (r c f g ^ {\prime}) \land \text { topology\_ok } (r c f g ^ {\prime})
$$

F.4.1 Proof Sketch. This is by far the most complex proof for this system. We proceed by induction on the well-formedness relation for Eff . The main problem here is the base cases, for which we first prove well-typedness and then the invariants capability_ok and topology_ok. The proof of well-typedness is done by a standard induction argument on the well-typedness relations for each part of the configuration. For the invariants we argue about the restrictions of each reference in the object graph, with regards to its capability. 

Each step of the region configuration will have a corresponding action (removal/addition of locations and references) on the region graph. Furthemore, because of well-typedness we can express this action using only the object graph $G ( r c f g )$ , the region ordering $\rho ( { \overline { { \Gamma } } } , r c f g )$ and $E \mathcal { f }$ . We use this fact and the constraints imposed by the reference capabilities to argue that the invariants hold in the resulting configuration. 

F.4.2 Proof of Theorem. 

Main 1 $r c f g \xrightarrow [ ] { E f f } r c f g ^ { \prime }$ 

Main 2 $\overline { { { \Gamma } } } \vdash E f f \ l { \ l { 1 } } \overline { { { \Gamma } } } ^ { \prime }$ 

Main 3 $\overline { { \Gamma } } ; \Delta ; \Psi \vdash r c f g$ 

Main 4 capability_ $. 0 \mathsf { k } ( r c f \mathsf { g } )$ 

Main 5 topology_ok(rcfg) 

We proceed by structural induction on the derivation of $\overline { { { \Gamma } } } \vdash E f f \ l { \ l { 1 } } \overline { { { \Gamma } } } ^ { \prime }$ 

Case 1 WF -EFF -CAST: 

From WF -EFF -CAST we get 

A1 Eff = cast(??, ??????, ?? ????) 

A2 $\overline { { \Gamma } } = \Gamma : : \overline { { \Gamma } } _ { 0 }$ 

A3 Γ ⊢ ?????? : ?? ⊣ Γ′ 

A4 ?? ?? <: ?? 

A5 $\overline { { \Gamma } } ^ { \prime } = \Gamma ^ { \prime } , x : t : \overline { { \Gamma } } _ { 0 }$ 

From REGION -STEP -CAST 

A6 $r c f g = \langle ( r , S , F ) : : R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

A7 get(use, ?? ) = ( (??, ??), ?? ′) 

A8 cfg_load $( ( r , S , F ) : \mathit { R S } , \mathit { H _ { o p } } * \mathit { H _ { c l } } * \mathit { H _ { f r } } , \mathit { l } ) = ( C , _ { - } )$ 

A9 ?? ′′ = ?? ′, ?? ↦→ (??, ??) 

A10 $r c f g ^ { \prime } = \langle ( r , S , F ^ { \prime \prime } ) : : R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

We apply lemma F.8, and get 

A11 $\Delta ; \Gamma ^ { \prime } \vdash F ^ { \prime }$ 

A12 ?? : CL ∈ Δ 

A13 ?? CL <: ?? 

We apply WF -F -CONS on A11 , A12 , A13 to get 

A14 $\Delta ; \Gamma ^ { \prime } , x : t \vdash F ^ { \prime \prime }$ 

The rest of the configuraton remains unchanged. Therefore we can conclude that 

A15 $\Gamma ^ { \prime } : : \overline { { \Gamma } } _ { 0 } ; \Delta ; \Psi \vdash r c f \mathrm { g } ^ { \prime }$ 

We now move to prove capability_ok $( r c f g ^ { \prime } )$ and topology_ok $( r c f g ^ { \prime } )$ . Given that $G ( r c f g ) =$ $\boldsymbol { \mathcal { G } } = ( \mathcal { L } , \boldsymbol { \mathcal { R } } ) , \boldsymbol { G } ( \boldsymbol { r } \boldsymbol { c } f \boldsymbol { g } ^ { \prime } ) = \boldsymbol { \mathcal { G } } ^ { \prime } = ( \mathcal { L } ^ { \prime } , \boldsymbol { \mathcal { R } } ^ { \prime } )$ it follows from F.14 that 

A16 $\mathcal { L } ^ { \prime } = \mathcal { L }$ 

$$
\text { A17 } \mathcal {G} ^ {\prime} = \left\{ \begin{array}{l l} \mathcal {G} + (\operatorname{root} (r) \xrightarrow {x , k} l o c _ {z}) & \text { if   use = z } \\ \mathcal {G} - (\operatorname{root} (r) \xrightarrow {z , k} l o c _ {z}) + (\operatorname{root} (r) \xrightarrow {x , k} l o c _ {z}) & \text { if   use = drop   z } \end{array} \right.
$$

A18 ref G (root(?? ), ??) = (root(?? ) ??,??−−→ loc?? ) 

A19 loc G (??) = loc?? 

By A10 , Main 4 , A18 

A20 $\rho = \rho ( \overline { { \Gamma } } , r c f g ) = \rho ( \overline { { \Gamma } } , r c f g ^ { \prime } )$ 

A21 Cl = Closed (rcfg) = Closed (rcfg′) 

${ \mathrm { A } } 2 2 \ { \mathrm { \it F r } } = { \mathrm { F r o z e n } } ( r c f g ) = { \mathrm { F r o z e n } } ( r c f g ^ { \prime } )$ 

A23 region_order1(??, Cl, Fr, root $( r ) { \xrightarrow { z , k } }$ loc?? ) 

A24 location_ok1 $( \boldsymbol { \ r o o t { ( r ) } } \xrightarrow { z , k } l o c _ { z } )$ 

A25 var_unique(?? (rcfg)) 

We proceed by cases of the value of ?? 

Case 1.a ?? = mut: 

– var_unique $( \mathcal { G } ^ { \prime } )$ because of O2 . 

– region_order(??, Cl, Fr, G′) By A23 , region_order1(??, Cl, Fr, root(?? )) ??,??−−→ $( \rho , C l , F r , \bar { g } ^ { \prime } )$ $( \rho , C l , F r , \mathsf { r o o t } ( r ) ) \xrightarrow { x , k }$ $l o c _ { z } )$ . It follows. 

– location_ok $( \mathcal { G } ^ { \prime } )$ By A24 , location_ok1 $( \mathsf { r o o t } ( r ) \xrightarrow { x , k } l o c _ { z } )$ . It follows. 

– deep_freeze $( F r , \mathcal { G } ^ { \prime } )$ since no frozen regions have changed. 

– topology_ok_graph $( \rho , F r , \theta ^ { \prime } )$ since ?? = mut implies $\rho \vdash r i d ( l o c _ { z } ) \leq r .$ 

– entrypoints_ok_graph $( \rho , \mathcal { G } ^ { \prime } )$ since we are not affecting entry points. 

Case 1.b ?? ∈ {paused, tmp}: Similar to Case 1.a . 

Case 1.c ?? = imm: Similar to Case 1.a , observing that Frozen $( r c f g ) = F r o z e n ( r c f g ^ { \prime } )$ and that capability_ok(rcfg) implies $r i d ( l o c _ { z } ) \in F r o z e n ( r c f g )$ . 

Case 1.d ?? = var: Similar to Case 1.a , with O3 to get var_unique $( { \mathcal { G } } ^ { \prime } )$ 

Case 1.e ?? = iso: Similar to previous cases, using O4 to get topology_ok_graph $( \rho , \mathcal { G } ^ { \prime } )$ 

Case 2 WF -EFF -NOCAST: Similar to Case 1 . 

Case 3 WF -EFF -SPLI T: By rule WF -EFF -SPLI T 

A1 $\overline { { \Gamma } } = \overline { { \Gamma } } [ \boldsymbol { x } : t | t ^ { \prime } ]$ 

A2 $\overline { { { \Gamma } } } [ x : t ] \vdash E f f \vdash 1 \overline { { { \Gamma } } } _ { 1 }$ 

A3 $\overline { { \Gamma } } [ \boldsymbol { x } : t ^ { \prime } ] \vdash E f f \vdash \overline { { \Gamma } } _ { 2 }$ 

A4 $\overline { { { \Gamma } } } ^ { \prime } = \overline { { { \Gamma } } } _ { 1 } | \overline { { { \Gamma } } } _ { 2 }$ 

By O7 , and WLOG 

A5 $\overline { { \Gamma } } [ \boldsymbol { x } : t ] \vdash r c f \dot { \boldsymbol { g } }$ 

By induction hypothesis 

A6 ${ \overline { { \Gamma } } } _ { 1 } \vdash r c f g ^ { \prime }$ 

A7 capability_ok $( r c f g ^ { \prime } )$ 

A8 topology_ok(rcfg′) 

By $\overline { { \Gamma } } [ \boldsymbol { x } : t ] \overset { \mathsf { p w } } { \subseteq } \overline { { \Gamma } } [ \boldsymbol { x } : t ^ { \prime } ]$ , O6 

A9 $\overline { { \Gamma } } _ { 1 } \overset { \mathtt { p w } } { \subseteq } \overline { { \Gamma } } _ { 2 }$ 

By ${ \mathrm { A } } 9 { \mathrm { ~ , ~ } } { \mathrm { A } } 6 { \mathrm { ~ , ~ } } { \mathrm { O } } 5$ 

A10 ${ \overline { { \Gamma } } } _ { 1 } | { \overline { { \Gamma } } } _ { 2 } \vdash r c f g ^ { \prime } .$ 

Case 4 WF -EFF -EN TER: By WF -EFF -EN TER 

A1 $E f f = e n t e r ( w , k , y . f , x _ { 1 } = u s e _ { 1 } , . . . , x _ { n } = u s e _ { n } )$ 

A2 $\overline { { \Gamma } } = \overline { { \Gamma } } _ { 1 } : : \overline { { \Gamma } } ^ { * }$ 

A3 $\forall i \in [ 1 , n ] . \Gamma _ { i } \vdash u s e _ { i } \mathrm { ~ + ~ } \Gamma _ { i + 1 }$ 

A4 $\Gamma _ { n + 1 } ( y ) = k ^ { \prime } ~ C L$ 

A5 open(??′) 

$\mathrm { A } 6 \ \mathbf { f t y p e } ( C L , f ) = t$ 

A7 cap(??????, ?? ) 

A8 ?? ∈ {tmp, var} 

A9 $k = \mathsf { v a r } \implies k ^ { \prime } = \mathsf { v a r }$ 

A10 ?? ′ = make_mut ( ()?? ) 

A11 $\forall i \in [ 1 , n ] . t _ { i } ^ { \prime } = \left\{ { t _ { i } } \atop { \mathsf { p a u s e d } } \odot t _ { i } \right.$ if $\mathsf { c a p } ( i s o , t _ { i } )$ otherwise 

$\mathrm { A } 1 2 \ \Gamma = x _ { 1 } : t _ { 1 } ^ { \prime } , . . . , x _ { n } : t _ { n } ^ { \prime }$ 

$\mathsf { A } 1 3 \ \Gamma ^ { \prime } = \Gamma , \boldsymbol { w } : k \mathsf { C e l } 1 [ t ^ { \prime } ]$ 

$A 1 4 ~ \overline { { \Gamma } } ^ { \prime } = \Gamma : : \Gamma _ { n + 1 } : : \overline { { \Gamma } } ^ { * }$ 

By REGION -EN TER -OK 

A15 $r c f g = \langle ( r , S , F _ { 1 } ) : : R S ; H _ { o p } ; ( r ^ { \prime } , S ^ { \prime } ) * H _ { c l } ; H _ { f r } \rangle$ 

A16 ?? fresh 

$\mathsf { A } 1 7 \ \forall i \in [ 1 , n ] . x _ { i }$ fresh 

$\mathrm { A } 1 8 \ \forall i \in [ 1 , n ] . \mathbf { g e t } ( F , u s e _ { i } ) = ( ( k _ { i } , \iota _ { i } ) , F _ { i + 1 } )$ 

$\mathrm { A 1 9 ~ } \forall i \in [ 1 , n ] ( k _ { i } ^ { \prime } , \iota _ { i } ) = \left\{ \begin{array} { l l } { ( k _ { i } , \iota _ { i } ) } & { } \\ { ( \mathrm { p a u s e d } \odot k _ { i } , \iota _ { i } ) ) } & { } \end{array} \right.$ ( (???? , ???? ) if $k _ { i } = \mathrm { i } s _ { 0 }$ otherwise 

A20 $F = [ x _ { i } \mapsto ( k _ { i } ^ { \prime } , \iota ) \ | \ i \stackrel { \cdot } { \in } [ 1 , n ] ]$ 

A21 $F _ { n + 1 } ( y ) = ( k _ { \mathrm { h e r e } } , \iota )$ 

A22 $\mathrm { c f g \_ l o a d } ( ( r , S , F _ { n + 1 } ) : \mathrel { \mathop : } R S , H _ { o p } , \iota ) = o [ f \mapsto ( k _ { \mathrm { b r i d g e } } , \iota ^ { \prime } ) ]$ 

A23 ??′ ∈ dom(?? ′) 

A24 ??′′ fresh 

A25 $F ^ { \prime } = F , w \mapsto ( k , \iota ^ { \prime \prime } )$ 

A26 $R F = ( r ^ { \prime } , [ \iota ^ { \prime \prime } \mapsto ( \mathsf { C e l 1 } , [ \mathsf { v a l } \mapsto ( \mathsf { m u t } , \iota ^ { \prime } ) ] ) ] , F ^ { \prime } )$ 

A27 $r c f g ^ { \prime } = \langle R F : : ( r , S , F _ { n + 1 } ) : : R S ; ( r ^ { \prime } , S ^ { \prime } ) * H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

By repeated application of lemma F.8 

A28 $\Delta ; \Gamma _ { n + 1 } \vdash F _ { n + 1 }$ 

A29 $\iota _ { i } : C L \in \Delta$ 

A30 $k _ { i } ~ C L _ { i } < : t _ { i }$ 

For each ?? we split into case on $k _ { i } { \mathrm { : } }$ 

Case 4.a ?? = iso: This, A30 , A11 , and O9 implies that 

B1 cap(iso, ???? ) 

By A11 and A19 , 

B2 $k _ { i } ^ { \prime } = k _ { i }$ 

B3 $t _ { i } ^ { \prime } = t _ { i }$ 

Thus by A30 , B2 , B3 

B4 $k _ { i } ^ { \prime } C L < : t _ { i } ^ { \prime }$ 

Case 4.b $k \neq$ iso: Similarly to first first subcase, A30 , A11 , O9 implies 

B1 $k _ { i } ^ { \prime } = \mathsf { p a u s e d } \odot k _ { i }$ 

B2 $t _ { i } ^ { \prime } = \mathsf { p a u s e d } \odot t _ { i }$ 

Specifically $t _ { i } ^ { \prime }$ is well-defined, and thus, by B1 , B2 , A30 , O8 

B3 $k _ { i } ^ { \prime } C L < : t _ { i } ^ { \prime }$ 

We conclude that 

C1 $\forall i \in [ 1 , n ] . k _ { i } ^ { \prime } C L < : t _ { i } ^ { \prime }$ 

Starting with the $\Delta ; \varepsilon \vdash \varepsilon ,$ by repeated application of WF -F -CONS, A12 , A20 , C1 

C2 $\Delta ; \Gamma \vdash F$ 

By O10 

C3 $\Delta ^ { \prime } = \Delta , \iota ^ { \prime \prime } : \mathsf { C e l } 1 [ t ^ { \prime } ]$ 

C4 $\Delta ^ { \prime } { ; } \Gamma \vdash F$ 

By WF -F -CONS, C4 , ?? Cell[?? ′] <: ?? Cell[?? ′] 

C5 $\Delta ^ { \prime } ; \Gamma ^ { \prime } \vdash F ^ { \prime }$ 

By A22 and well-formedness of rcfg 

C6 $o [ f \mapsto ( k _ { \mathrm { b r i d g e } } , \iota ^ { \prime } ) ] = ( \# C L , F ^ { * } )$ 

C7 $\iota : \ C L \in \Delta$ 

C8 $\Delta ; { \bf f t y p e s } ( ( ) ~ C L ) \vdash F ^ { * }$ 

C9 $\iota ^ { \prime } : \ C L ^ { \prime } \in \Delta$ 

C10 $k _ { \mathrm { b r i d g e } } \ C L ^ { \prime } < : t$ 

By A7 and C10 

C11 $k _ { \mathrm { b r i d g e } } = \mathrm { i } { \mathrm { s o } }$ 

By C10 , O11 , 

C12 mut CL′ <: make_mut ( ()?? ) = ?? ′ 

By $\mathrm { C 3 , C L T A G \mathrm { - } C E L L , \ C 1 2 }$ 

C13 $\# \mathsf { C e l l } < \# : \mathsf { C e l l } [ t ^ { \prime } ]$ 

C14 $\iota ^ { \prime \prime } : \mathsf { C e l l } [ t ^ { \prime } ] \in \Delta ^ { \prime }$ 

C15 $\Delta ^ { \prime } ; \mathsf { v a l } : t ^ { \prime } \vdash \mathsf { v a l } \mapsto \left( \mathsf { m u t } , { \iota } ^ { \prime } \right)$ 

C16 $\Delta ; \varepsilon \vdash \varepsilon$ 

By $\mathrm { C 1 3 ~ - ~ C 1 6 ~ , w F { \cdot } S U B H E A P { \cdot } C O N S } ,$ 

C17 $\Delta ; \iota ^ { \prime \prime } : \mathsf { C e l 1 } \left[ t ^ { \prime } \right] \vdash \iota ^ { \prime \prime } \mapsto ( \# \mathsf { C e l 1 } , \mathsf { v a l } \mapsto ( \mathsf { m u t } , \iota ^ { \prime } )$ 

From $\mathrm { C 1 7 ~ , ~ C 5 ~ , ~ A 2 8 }$ , and Main 3 , it follows that there is $\Delta _ { R S } , \Psi _ { R S }$ such that 

C18 $\Gamma ^ { \prime } : : \overline { { { \Gamma } } } _ { 1 } ; { \Delta } ^ { \prime } ; { \Delta } _ { R S } , { \iota } ^ { \prime \prime } : C e l l [ { t } ^ { \prime } ] ; { r } ^ { \prime } , \Psi _ { R S } \vdash R F : : ( r , S , F _ { n + 1 } ) : : R S$ 

By applying rules for well formedness, we end up with 

C19 $\overline { { \Gamma } } ^ { \prime } ; \Delta ^ { \prime } ; \Psi \vdash r c f g ^ { \prime }$ 

Now, we move on to capability_ok(rcfg′). 

We write $\begin{array} { r } { G = G ( r c f g ) } \end{array}$ and $\mathcal { G } ^ { \prime } = G ( r c f g ^ { \prime } )$ . Because of well-formedness of rcfg and rcfg′, these are well-formed. 

We write 

$- \ \rho = \rho ( \overline { { \Gamma } } , r c f g ) = r : : \rho ^ { * }$ 

$\bar { \mathbf { \xi } } - \mathbf { \nabla } \rho ^ { \prime } = \rho ( \overline { { \Gamma } } ^ { \prime } , r c f g ^ { \prime } ) = r ^ { \prime } \mathbf { \xi } _ { \underset { t ^ { \prime } } { : } } r : \mathbf { \nabla } \rho ^ { * }$ ??.?? 

We let $\mathcal { G } _ { i }$ be defined as follows 

$- \mathcal { G } _ { 0 } = \mathcal { G } + \mathsf { r o o t } ( r ^ { \prime } ) + \mathsf { t e m p } ( r ^ { \prime } , \iota ^ { \prime \prime } )$ 

$- \ r e f _ { i } = \Gamma { \ o { 0 0 } { 1 } } ( r ) \ \xrightarrow { z _ { i } , k _ { i } } \ l o c _ { i } = r e f _ { G _ { i } } ( r { \ o { 0 0 } { 1 0 } } \ / x ) , z _ { i } )$ 

$\ - \ r e f _ { i } ^ { \prime } = r \cot ( r ^ { \prime } ) \ \xrightarrow { x _ { i } , k _ { i } ^ { \prime } } \ l o c _ { i }$ 

$$
\begin{array}{l} - k _ {i} ^ {\prime} = \left\{ \begin{array}{l l} k _ {i} & \text { if } k _ {i} = \text { iso } \\ \text { paused } \odot k _ {i} & \text { otherwise } \end{array} \right. \\ - \mathcal {G} _ {i + 1} = \mathcal {G} _ {i} - r e f _ {i} + r e f _ {i} ^ {\prime} \text {   if   } u s e _ {i} = \mathbf {d r o p} z _ {i}. \\ - \mathcal {G} _ {i + 1} = \mathcal {G} _ {i} + r e f _ {i} ^ {\prime} \text {   if   } u s e _ {i} = z _ {i}. \\ \end{array}
$$

Then, from lemma F.14, 

$$
\text { C20 } \mathcal {G} ^ {\prime} = \mathcal {G} _ {n} + \operatorname{root} (r ^ {\prime}) \xrightarrow {w , k} \operatorname{temp} (r ^ {\prime}, \iota^ {\prime \prime}) + \operatorname{temp} (r ^ {\prime}, \iota^ {\prime \prime}) \xrightarrow {\text { val,mut }} \operatorname{heap} (r ^ {\prime}, \iota^ {\prime})
$$

We make some general observations: 

If we have $\mathcal { G } , \rho , C l , F r , r$ such that ⊢ $\rho , C l \cup \{ r \}$ , Fr and $r \not \in$ Cl then 

OBS1 If 

$$
- \text { region\_order1 } (\rho , C l \cup \{r \}, F r, l o c _ {1} [ r _ {1} ] \xrightarrow {f , k} l o c _ {2} [ r _ {2} ])
$$

$( r : : \rho , C l , F r , l o c _ { 1 } [ r _ { 1 } ] \stackrel { f , k } { \longrightarrow } l o c _ { 2 } [ r _ { 2 } ] )$ 

Follows by 

(i) if ?? ≠ iso, moving ?? from the closed set to region stack does not affect ordering of $r _ { 1 }$ and $r _ { 2 }$ 

(ii) If $k = \mathrm { i } s 0 ;$ , then either $r _ { 2 } = r ^ { \prime }$ in which case topology_ok(rcfg) implies $r _ { 1 } = r _ { \mathrm { { \mathrm { i } } } }$ thus $r : \rho \vdash r _ { 1 } < r _ { 2 }$ . 

Otherwise, $r _ { 2 } \neq r ^ { \prime }$ and region order of $r _ { 1 }$ and $r _ { 2 }$ will be unaffected. 

This in turn implies 

OBS2 If region_order $( \rho , C l \cup r , F r , G )$ , then region_order $( r : \rho , C l , F r , G )$ . 

OBS3 If 

$$
- \text {   entrypoints\_ok } (r:: \rho , \mathcal {G})
$$

$$
- r ^ {\prime} \notin r:: \rho
$$

$$
- \operatorname{root} (r) \xrightarrow {y , k _ {y}} \operatorname{loc} _ {y} \in \mathcal {G}
$$

$$
- l o c _ {y} \xrightarrow {f ^ {\prime} , k _ {f}} \operatorname{heap} \left(r ^ {\prime}, \_ \right.) \in \mathcal {G}
$$

Then entrypoints_ok $( r ^ { \prime } \underset { y . f } { : : } r \mathrel { : : } \rho , \mathcal G )$ 

Follows from definition of entrypoints_ok. 

OBS4 topology_ok_graph $( \rho , F r , \mathcal { G } )$ implies topology_ok_graph $( r : \rho , F r , \mathcal { G } )$ . 

Follows from that the relation on region ids defined by $\rho \vdash r 1 \le r 2$ is included in the relation defined by $r : \rho \vdash r 1 \leq r 2 .$ . 

OBS5 If 

$$
- \text { region\_order1 } (r:: \rho , C l, F r, l o c _ {1} [ r _ {1} ] \xrightarrow {f , k} l o c _ {2} [ r _ {2} ])
$$

$$
- r _ {1} \in \rho
$$

$$
- k \neq \text { iso }
$$

Then region_order1 $( r : : \rho , C l , F r , l o c [ r ] \xrightarrow { f ^ { \prime } , \mathrm { p a u s e d } \odot k } l o c _ { 2 } [ r _ { 2 } ] )$ 

Follows from case analysis on ??. 

OBS6 If 

(i) region_orde $\cdot ( r ^ { \prime } : : r : : \rho , C l , F r , \mathsf { r o o t } ( r ) \xrightarrow { f , \mathsf { i s o } } l o c _ { 2 } [ r _ { 2 } ] )$ 

(ii) $r _ { 2 } \neq r ^ { \prime }$ 

Then region_orde $\cdot ( r ^ { \prime } : : r : : \rho , C l , F r , r \mathrm { o o t } ( r ^ { \prime } ) \xrightarrow { f ^ { \prime } , \mathrm { i } \mathrm { s o } } l o c _ { 2 } [ r _ { 2 } ] )$ ?? ′,iso−−−−→ loc2 [??2]) 

Follow by the following argument: (i) and capability iso implies that $r _ { 2 } \neq r .$ . Furthermore $r _ { 2 } \neq r ^ { \prime }$ by (ii), so we do not have $r ^ { \prime } : \boldsymbol { r } : : \boldsymbol { \rho } \vdash r < r _ { 2 }$ . Also, $r \not \in F r$ . Thus $r _ { 2 } \in C l ,$ and thus the conclusion holds. 

OBS7 If 

(i) location_ok1 $( l o c _ { 1 } \xrightarrow { f , k } l o c _ { 2 } )$ 

(ii) ?? ≠ iso 

Then 

– location_ok1(root(?? ) ?? ,paused ⊙ ??−−−−−−−−−→ loc2) 

Follows from case analysis on ??. 

OBS8 Similary, if 

(i) location_ok1 $( l o c _ { 1 } \xrightarrow { f , \mathrm { i s o } } l o c _ { 2 } )$ 

– location_ok1 $( \Gamma 0 0 \ t ( r ) \xrightarrow { f , \mathrm { i } s 0 } l o c _ { 2 } )$ 

Because of these observations, and since adding locations does not affect invariants we have 

E1 var_unique(G0) 

E2 region_order $( \rho ^ { \prime } , C l ^ { \prime } , F r , \mathcal { G } _ { 0 } )$ 

E3 location_ok(G0) 

E4 deep_freeze $( F r , \mathcal { G } _ { 0 } )$ 

E5 entrypoints_ok_graph $( \rho ^ { \prime } , \mathcal { G } _ { 0 } )$ using well-formedness to get $( \mathsf { r o o t } ( r ) \xrightarrow { y , k ^ { \prime } } l o c _ { y } ) \in \mathcal { G } _ { 0 } .$ ??,??′ $( l o c _ { y } \xrightarrow { f , \mathrm { i s o } } \mathsf { h e a p } ( r ^ { \prime } , \iota ^ { \prime } ) ) \in \mathcal { G } _ { 0 }$ and that open(??′) implies $r i d ( l o c _ { y } ) \in \rho .$ . 

E6 topology_ok_graph(??′, Fr, G0) 

By an inductive argument on ??, by cases on $k _ { i } { : }$ 

Case 4.a ?? = var: Implies $u s e _ { i } = \mathbf { d r o p } \ : z _ { i } ,$ , and thus $\mathcal { G } _ { i } = \mathcal { G } _ { i - 1 } - r e f _ { i } + r e f _ { i } ^ { \prime } .$ . 

– var_unique(G?? ) follows from O3 . 

– region_order $\left( \rho ^ { \prime } , C l ^ { \prime } , F r , \mathcal { G } _ { i } \right)$ follows from OBS5 

– location_ok(G??) follows from OBS7 

– deep_freeze(Fr, G?? ) follows from ?? ′ ∉ Fr and deep_freeze $( \mathcal { G } _ { i - 1 } )$ 

– entrypoints_ok(??′, G??) since no entry points have changed, 

– topology_ok_graph(??′, Fr, G?? ) follows from $k _ { i } = \mathsf { p a u s e d }$ and region_order(G??) 

Case 4.b ?? = iso: Implies $u s e _ { i } = \mathbf { d r o p } \ : z _ { i } ,$ , and thus $\mathcal { G } _ { i } = \mathcal { G } _ { i - 1 } - r e f _ { i } + r e f _ { i } ^ { \prime } .$ 

– var_unique(G?? ) follows from O2 . 

– region_order(??′, Cl′, Fr, G?? ) follows from $r i d ( l o c _ { i } ) \neq r ^ { \prime }$ (from topology_ok_graph(??′, Fr, G?? )) and OBS6 . 

– location_ok(G??) follows from OBS8 

– deep_freeze(G??) follows from ?? ′ ∉ Fr and deep_freeze $\left( \mathcal { G } _ { i - 1 } \right)$ 

– entrypoints_ok $( \rho ^ { \prime } , \mathcal { G } _ { i } )$ since no entry points have changed, 

– topology_ok_graph $( \rho ^ { \prime } , F r , \mathcal { G } _ { i } )$ follows from O4 and that $r i d ( l o c _ { i } ) ~ \in ~ C l$ (which implies that we do not have $\rho ^ { \prime } \vdash r i d ( l o c _ { i } ) \le r )$ . 

From a simple case analysis on ??, and the observation that there is no reference ref in ${ \mathcal { G } } _ { n }$ such that ref points to temp $( r ^ { \prime } , \iota ^ { \prime \prime } )$ , we have 

– var_unique(G′) 

– region_order $( \rho ^ { \prime } , C l ^ { \prime } , F r , \mathcal { G } ^ { \prime } )$ 

– location_ok(G′) 

– deep_freeze(Fr, G′) 

– topology_ok_graph(??′, Cl′, Fr, G′) 

Meaning 

– capability_ok(rcfg′) 

– topology_ok(rcfg′) 


Case 5 WF -EFF -SWAP:


We have
A1 Eff = swap(x, y.val, use)
A2  \( \overline{\Gamma} = \Gamma :: \Gamma^{*} \) 
A3  \( \Gamma \vdash use : t \dashv \Gamma'[y : var Cell[t']] \) 
A4 cap( \( \{var\}^{c}, t \) )
A5  \( \overline{\Gamma}' = \Gamma'[y : var Cell[t]], x : t :: \Gamma^{*} \) 
- G = G(rcfg)
-  \( \rho = \rho(rcfg) \) 
- Cl = Closed(rcfg)
- Fr = Frozen(rcfg)

There are two cases for the dynamic execution of Eff: REGION-SWAP-HEAP and REGION-SWAP-TEMP. However, the former is impossible because of well-formedness and capability_ok(rcfg), specifically location_ok(G) and region_order( \( \rho, Cl, Fr, G \) ), which says that y with var capability must point to the temporary store in the top region frame.
By REGION-SWAP-TEMP
A6 rcfg =  \( \langle(r, S, F) :: RS; H_{op}; H_{cl}; H_{fr} \rangle \) 
A7 x fresh
A8 get(F, use) = ((k, l), F')
A9  \( F'(y) = (k_y, l_y) \) 
A10 load(S,  \( l_y \) ) = o[val  \( \mapsto (k', l') \) ]
A11 o' = o[val  \( \mapsto (k, l) \) ]
A12 store(S, l, o') = S'
A13  \( F'' = F', x \mapsto (k', l') \) 
A14 rcfg' =  \( \langle(r, S', F'') :: RS; H_{op}; H_{cl}; H_{fr} \rangle \) 
We note that
A15  \( \rho' = \rho(rcfg') = \rho \) 
A16  \( Cl' = Closed(rcfg') = Cl \) 
A17  \( Fr' = Closed(rcfg') = Fr \) 
By well-formedness of rcfg we have
A18  \( \Delta; \Delta_S \vdash S \) 
Using lemma F.8, we know
A19  \( \Delta; \Gamma'[y : var Cell[t']] \vdash F' \) 
We argue that there is a  \( \Delta' \)  such that  \( \overline{\Gamma'}; \Delta'; \Psi \vdash rcfg \) . We know  \( i_y : Cell[\_] \)  is in  \( \Delta \)  since rcfg is well typed. Let
A20  \( \Delta = \Delta^{*}[i_y : Cell[\_]] \) 
A21  \( \Delta' = \Delta^{*}[i_y : Cell[t]] \) 
By induction on the structure of the derivation on A19
A22  \( \Delta'; \Gamma'[y : var Cell[t]] \vdash F'' \) 
By induction on the structure of the derivation of A18
A23  \( \Delta_S = \Delta_S^{*}[i_y : Cell[\_]] \) 
A24  \( \Delta_S' = \Delta_S^{*}[i_y : Cell[t]] \) 
A25  \( \Delta'; \Delta_S' \vdash S' \) 
For any frame (_,_, F₁) ∈ RS there is a corresponding  \( \Gamma_1 \in \overline{\Gamma}^* \) . By induction on structure of  \( \Delta; \Gamma_1 \vdash F_1: if x \mapsto (k_x, i_x) \in F_1, and corresponding typing x : t_x, then i_x ≠ i_y, since otherwise we would contradict var_unique(G). We conclude that i_x : CL ∈  \( \Delta \implies i_x : CL \in \Delta' \)  and thus k_x CL <: t_x. Thus by WF-F-CONS we can conclude that 

A26 $\Delta ^ { \prime } ;  { \Gamma _ { 1 } } \vdash F _ { 1 }$ 

A analogous argument holds for any $( \# C L , F _ { 2 } )$ in $H _ { o p } * H _ { c l } * H _ { f r } $ , with rule WF -SUBHEAP -CONS. 

Thus all subheaps and frames are well-formed under $\overline { { { \Gamma } } } ^ { \prime } ; \Delta ^ { \prime } ; \Psi ,$ , allowing us to conclude that 

A27 $\overline { { \Gamma } } ^ { \prime } ; \Delta ^ { \prime } ; \Psi \vdash \overline { { r } } c f g ^ { \prime }$ 

Letting $\mathcal { G } = G ( r c f g )$ , by lemma F.14 

$$
\text {A28} \mathcal {G} ^ {\prime} = \left\{ \begin{array}{l l} \mathcal {G} - (\operatorname{root} (r) \xrightarrow {z , k _ {z}} \operatorname{loc} _ {z}) - (\operatorname{loc} _ {y} \xrightarrow {f , k _ {f}} \operatorname{loc} _ {f}) + (\operatorname{loc} _ {y} \xrightarrow {f , k _ {z}} \operatorname{loc} _ {z}) & \text {if use = drop z} \\ \mathcal {G} - (\operatorname{loc} _ {y} \xrightarrow {f , k _ {f}} \operatorname{loc} _ {f}) + (\operatorname{loc} _ {y} \xrightarrow {f , k _ {z}} \operatorname{loc} _ {z}) & \text {otherwise} \end{array} \right.
$$

– var_unique $( \mathcal { G } ^ { \prime } )$ holds because of O2 and O3 , because the only way to break the invariant is if $k _ { z } ~ = { \mathrm { v a r } }$ . But this implies use = drop ?? and thus we remove the old reference to $l o c _ { z } .$ 

– region_order $( \rho , C l , F r , G ^ { \prime } )$ holds because region $r i d ( \mathsf { r o o t } ( r ) ) = r i d ( l o c _ { y } )$ , i.e. the new reference will have the same relation between start and end region. 

– location_ok $( \mathcal { G } ^ { \prime } )$ follows from a case analysis on $k _ { z } ,$ , notin $k _ { z } \neq$ var by A3 and A4 . 

– deep_freeze $( F r , \mathcal { G } ^ { \prime } )$ holds since $r \not \in F r .$ 

– entrypoints_ok $( \rho , \mathcal { G } ^ { \prime } )$ since no entrypoints have changed. 

– topology_ok_graph $( \rho , F r , \theta ^ { \prime } )$ holds by case analysis on $k _ { z }$ , using region_order $( \rho , C l , F r , \mathcal { G } ^ { \prime } )$ and O4 . 

Case 6 WF -EFF -SWAP -CLASS: We have 

A1 Eff = swap(??, ??.?? , use) 

A2 ${ \overline { { \Gamma } } } = \Gamma : : \Gamma ^ { * }$ 

A3 Γ ⊢ use : ?? ⊣ Γ′ 

A4 cap({var}??, ?? ) 

A5 $\Gamma ( y ) = k ~ C L$ 

A6 ?? ∈ {mut, tmp} 

A7 $\overline { { { \Gamma } } } ^ { \prime } = \Gamma ^ { \prime } [ y : k C L ] , x : t : : \Gamma ^ { * }$ 

This case is similar to case Case 5 above. One thing to note is that the case where $k = \mathrm { m u t }$ together with ⊢ ?? CL (from lemmas F.10, F.11) implies that the reference specified by use will be either imm, iso or mut, (i.e. a reference to a closed, frozen or within the same region) which will allow us to prove capability_ok and topology_ok. 

Case 7 WF -EFF -EXI T: 

We have 

A1 Eff = exit(??, use, ??.?? , ?? .??) 

A2 $\overline { { { \Gamma } } } = \Gamma ^ { \prime } : : \Gamma [ y : k { \cal C } L ] : \overline { { { \Gamma } } } ^ { * }$ 

A3 ?? ∈ {var, tmp, mut, paused} 

A4 Γ′ ⊢ use : ?? ⊣ Γ′′ 

A5 cap({iso, imm}, ?? ) 

A6 $\Gamma ^ { \prime \prime } ( w ) = k ^ { \prime } C L ^ { \prime }$ 

A7 ftype( $C L ^ { \prime } , g ) = t ^ { \prime }$ 

A8 cap(mut, ?? ′) 

A9 cap(iso, ftype( CL, ?? )) 

$$
\begin{array}{l} \text { A10 } k ^ {\prime \prime} C L ^ {\prime \prime} = \left\{ \begin{array}{l l} \text { varCell } [ m a k e \_ i s o (() t ^ {\prime}) ] & \text { if } k C L = \text { varCell } [ \_ ] \\ k C L & \text { otherwise } \end{array} \right. \\ \text { A11 } \overline {{\Gamma}} ^ {\prime} = \Gamma [ y: k ^ {\prime \prime} C L ^ {\prime \prime} ], x: t:: \overline {{\Gamma}} ^ {*} \\ \end{array}
$$

For the dynamic rule we have two cases: REGION -EXI T-TEMP and REGION -EXI T-HEAP. 

Case 7.a REGION -EXI T-TEMP: 

B1 $r c f g = \langle ( r ^ { \prime } , S ^ { \prime } , F ^ { \prime } ) : : ( r , S , F ) : : R S ; ( r ^ { \prime } , S _ { o p } ) * H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

B2 ?? fresh 

B3 $\mathbf { g e t } ( F ^ { \prime } , u s e ) = \left( ( k _ { z } , \iota _ { z } ) , F ^ { \prime \prime } \right)$ 

B4 $F ^ { \prime \prime } ( w ) = ( k _ { w } , \iota ^ { \prime } )$ 

B5 load $( S ^ { \prime } , \iota ^ { \prime } ) = o ^ { \prime } [ g \mapsto ( k _ { g } , \iota ^ { \prime \prime } ) ]$ 

B6 $F ( y ) = ( k _ { y } , \iota )$ 

B7 stack_load $| ( ( r , S , F ) : \ R S , \iota ) = o [ f \mapsto ( k _ { f } , \iota _ { f } ) ]$ 

B8 $s \mathrm { t a c k \_ s t o r e } ( ( r , S , F ) : \mathit { R S , 1 , o } [ f \mapsto ( k _ { f } , \check { \iota } ^ { \prime \prime } ) ] ) = ( r , S ^ { \prime \prime } , F ) : \because \mathit { R S ^ { \prime \prime } }$ 

B9 $F ^ { \prime \prime \prime } = F , x \mapsto \left( k _ { z } , \iota _ { z } \right)$ 

B10 $r c f g ^ { \prime } = \langle ( r , S ^ { \prime \prime } , F ^ { \prime \prime \prime } ) : : R S ^ { \prime } ; H _ { o p } ; ( r ^ { \prime } , S _ { o p } ) * H _ { c l } ; H _ { f r } \rangle$ 


Case 7.b REGION -EXI T-HEAP:


C1 rcfg = ⟨(r', S', F') :: (r, S, F) :: RS; (r', S_op) * H_op; H_cl; H_fr⟩
C2 x fresh
C3 get(F', use) = ((k_z, l_z), F'')
C4 F''(w) = (k_w, l')
C5 load(S', l') = o' [g ↦ (k_g, l'')]
C6 F(y) = (k_y, l)
C7 heap_load(H_op, l) = o[f ↦ (k_f, l_f)]
C8 heap_store(H_op, l, o[f ↦ (k_f, l'')] = H'_op
C9 F''' = F, x ↦ (k_z, l_z)
C10 rcfg' = ⟨(r, S, F''') :: RS; H'_op; (r', S_op) * H_cl; H_fr⟩ 


We note


D1 $\mathcal{G}=G(rcfg)$ D2 $\rho=\rho(rcfg)=r^{\prime}::r::\rho^{*}$ D3 $\rho^{\prime}=\rho(rcfg^{\prime})=r::\rho^{*}$ D4 Fr = Frozen(rcfg) = Frozen(rcfg')
D5 Cl = Closed(rcfg)
D6 Cl = Closed(rcfg') = Cl ∪ {r'}
D7 $\mathcal{G}^{\prime}=G(rcfg^{\prime})$ D8 var_unique(G)
D9 region_order(ρ, Cl, Fr, G) 


D10 location_ok(G)


D11 deep_freeze(G)
D12 entrypoints_ok_graph(ρ, G)
D13 topology_ok_graph(ρ, Fr, G) 

We note the similarity between Case 7.a and Case 7.b , and furthermore note that the they are similar to a swap(), with the extra action of popping a region frame. 

region_order $( \rho , C l , F r , r c f g )$ , topology_ok_graph $( \rho , \mathcal { G } )$ , location $\operatorname { o k } ( { G } )$ , and entrypoints_o $\iota ( \rho , r c f g )$ implies that the only reference from another region into $r ^ { \prime }$ is loc $_ y \xrightarrow { f , \mathrm { i s o } }$ heap $( r ^ { \prime } , \underline { { \mathbf { \Pi } } } )$ . Thus we can construct $\Delta ^ { \prime }$ from $\Delta$ by removing all $\iota ^ { * } \in \Delta$ such that $\iota ^ { * } \in \bar { S } ^ { \prime }$ . By these facts, we can conclude 

D14 $\Gamma [ y : k ~ C L ] : \overline { { \Gamma } } ^ { * } ; \Delta ^ { \prime } ; \Psi \vdash \langle ( r , S , F ) : : R S ; H _ { o p } ; ( r ^ { \prime } , S _ { o p } ) * H _ { c l } ; H _ { f r } \rangle .$ 

From similar reasoning to the swap effects e.g. Case 5 above. 

D15 $\overline { { \Gamma } } ^ { \prime } ; \Delta ^ { \prime } ; \Psi \vdash r c f g ^ { \prime }$ 

By lemma F.14, and well-formedness 

$$
\begin{array}{l} \text { D16 } \mathcal {G} ^ {\prime} = \mathcal {G} - \operatorname{root} (r ^ {\prime}) - \{\operatorname{temp} (r ^ {\prime}, \_) | \operatorname{temp} (r ^ {\prime}, \_) \in \mathcal {G} \} \\ - \left\{\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {- - } _ {-} \mid \operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {- - } _ {-} \in \mathcal {G} \right\} \\ - \left\{\operatorname{temp} (r ^ {\prime}, \_) \xrightarrow {\text {   -   }} \_ \mid \operatorname{temp} (r ^ {\prime}, \_) \xrightarrow {\text {   -   }} \_ \in \mathcal {G} \right\} \\ + \operatorname{root} (r) \xrightarrow {x , k _ {z}} l o c _ {z} \\ - \operatorname{loc} _ {y} \xrightarrow {f , \text { iso }} \operatorname{heap} \left(r ^ {\prime}, \_ \right.) + \operatorname{loc} _ {y} \xrightarrow {f , \text { iso }} \operatorname{heap} \left(r ^ {\prime}, l ^ {\prime \prime}\right) \\ \end{array}
$$

D17 use = ?? or use = drop ??. 

D18 $r e f _ { \mathcal { G } } ( \mathsf { r o o t } ( r ^ { \prime } ) , z ) = \mathsf { r o o t } ( r ^ { \prime } ) \xrightarrow { z , k _ { z } } l o c _ { z }$ 

D19 $r e f _ { \mathcal { G } } ( \mathsf { r o o t } ( r ) , y ) = \mathsf { r o o t } ( r ) \xrightarrow { y , k _ { y } } l o c _ { y }$ loc?? 

D20 $r e f _ { \mathcal { G } } ( l o c _ { y } , f ) = l o c _ { y } \xrightarrow { f , \mathsf { i s o } } \mathsf { h e a p } ( r ^ { \prime } , \digamma _ { - } )$ 

D21 $k _ { z } \in \{ \mathrm { i m m } , \mathrm { i } s \mathrm { o } \}$ 

D22 $k _ { y } \in \{ \mathrm { m u t } , \mathrm { p a u s e d } , \mathrm { v a r } , \mathrm { t m p } \}$ 

D23 ref G (root(?? ′), ?? ) = root(?? ′) ??,????−−−−→ temp(?? ′, ??′) 

D24 $k _ { w } \in \{ \mathrm { t m p } , \mathrm { v a r } \}$ 

$r e f _ { \mathcal { G } } ( \mathsf { t e m p } ( r ^ { \prime } , \iota ^ { \prime } ) , g ) = \mathsf { t e m p } ( r ^ { \prime } , \iota ^ { \prime } ) \xrightarrow { g , \mathsf { m u t } } \mathsf { h e a p } ( r ^ { \prime } , \iota ^ { \prime \prime } )$ 

– var_unique(G′) by applying observation O1 , and then using well-formedness and $\mathsf { c a p } ( \mathsf { i s o } , \mathbf { f t y p e } ( \mathbf { \Lambda } C L , f ) )$ to get $k _ { f } \neq$ var and $\mathbf { c a p } ( \{ \mathsf { i } \mathsf { s o } , \mathsf { i m m } \} , t )$ to get $k _ { z } \neq \mathsf { v a r } ,$ , and then applying observation O2 . 

– region_order $( \rho ^ { \prime } , C l ^ { \prime } , F r , \mathcal { G } ^ { \prime } )$ by D21 implying $r i d ( l o c _ { z } ) ~ \in ~ C l \cup F r \subsetneq ~ C l ^ { \prime } \cup F r$ thus region_order1 $( \rho ^ { \prime } , C l ^ { \prime } , F r , \mathsf { r o o t } ( r ) \xrightarrow { x , k _ { z } } l o c _ { z } )$ ??,????−−−→ loc?? ). Furthermore ?? ′ ∈ Cl′ and so $r ^ { \prime } \in C l ^ { \prime }$ region_order1 $( \rho ^ { \prime } , C l ^ { \prime } , F r , l o c _ { y } \xrightarrow { f , \mathrm { i } s \mathrm { o } }$ heap(?? ′, ??′′)) 

– location_ok(G′) by case analysis on $k _ { z }$ 

– deep_freeze $( F r , \mathcal { G } ^ { \prime } )$ since frozen regions are unchanged. 

– entrypoints_ok $( \rho ^ { \prime } , \mathcal { G } ^ { \prime } )$ by arguing that all region ids in $\rho$ are unique (by well-typedness of rcfg) and thus by topology_ok_graph $( \rho , \mathcal { G } )$ and O12 all entrypoint are disjoint and therefore the swap at $l o c _ { y } ,$ field $f ,$ , does not affect any other entrypoint. 

– topology_ok_graph(??′, Fr, G′) by O4 . 

Case 8 WF -EFF -FREEZE: 

We have 

A1 Eff = freeze( ()??, use) 

A2 $\overline { { \Gamma } } = \Gamma : \overline { { \Gamma } } ^ { * }$ 

A3 Γ ⊢ use : ?? ⊣ Γ′ 

A4 cap(iso, ?? ) 

A5 Γ′ = Γ′, ?? : make_imm(?? ) :: Γ∗ 

By REGION -FREEZE 

$\begin{array} { r } { \mathrm { A } 6 \ r c f g = \langle ( r , S , F ) : : R S ; H _ { o p } ; ( R * H ) * H _ { c l } ; H _ { f r } \rangle } \end{array}$ 

A7 ?? fresh 

A8 get(?? , use) = ( (??, ??), ?? ′) 

A9 ?? ∈ dom(??.??) 

A10 ?? = reachable_regions(??, ?????? ∗ ?? ∗ ?????? ) 

A11 ?? ′′ = ?? ′, ?? ↦→ (imm, ??) 

$\begin{array} { r } { \mathrm { A 1 2 } \ r c f { \mathrm { g } ^ { \prime } } = \langle \left( r , S , F ^ { \prime \prime } \right) : : \ R S ; H _ { o p } ; H _ { c l } ; \left( R * H \right) * H _ { f r } \rangle } \end{array}$ 

Well-typedness follows by induction on the well-formedness of the top region frame. We have 

A13 $\overline { { \Gamma } } ^ { \prime } ; \Delta ; \Psi \vdash r c f g ^ { \prime }$ 

We note 

A14 $\rho = \rho ( \overline { { \Gamma } } , r c f g ) = \rho ( \overline { { \Gamma } } ^ { \prime } , r c f g ^ { \prime } )$ 

A15 Cl = Closed (rcfg) 

A16 Fr = Frozen(rcfg) 

A17 $F r _ { n e w } = r i d s ( R * H )$ 

A18 $C l ^ { \prime } = C l - F r _ { n e w } = C l o s e d ( r c f g ^ { \prime } )$ 

$\mathrm { A 1 9 ~ } F r ^ { \prime } = F r + F r _ { n e w } = F r o z e n ( r c f g ^ { \prime } )$ 

$\mathrm { A } 2 0 \ G = G ( r c f g )$ 

A21 $\mathcal { G } ^ { \prime } = G ( r c f g ^ { \prime } )$ 

A22 use = drop ?? 

By lemma F.14, 

$\mathcal { G } ^ { \prime } = \mathcal { G } - \left( \mathsf { r o o t } ( r ) \xrightarrow { z , k _ { z } } l o c _ { z } [ r ^ { \prime } ] \right) + \left( \mathsf { r o o t } ( r ) \xrightarrow { x , \mathsf { i m m } } l o c _ { z } [ r ^ { \prime } ] \right)$ 

A24 $k _ { z } = \mathrm { i } s _ { 0 }$ 

– var_unique $( { \mathcal { G } } ^ { \prime } )$ since we do not handle var references. 

– region_order $( \rho , C l ^ { \prime } , F r ^ { \prime } , Q ^ { \prime } )$ by noting that reachable_regions says that all references between two regions reachable from ?? will have both ends in $R * H ,$ , and that ?? is in $R * H .$ 

– location_ok(G′) noting that $l o c _ { z } [ r ^ { \prime } ] = \mathsf { h e a p } ( r ^ { \prime } , \underline { { \boldsymbol { \mathbf { \Pi } } } } )$ by location $\operatorname { \lrcorner } \operatorname { o k } ( { \mathcal { G } } )$ and $k _ { z } = \mathrm { i } s 0 .$ 

– deep_freeze $( F r ^ { \prime } , \mathcal { G } ^ { \prime } )$ by similar argument to reachable_regions. 

– topology_ok $g r a p h ( \rho , F r ^ { \prime } , \mathcal { G } ^ { \prime } )$ since $F r \subseteq F r ^ { \prime }$ and $\boldsymbol { r } ^ { \prime } \in F \boldsymbol { r } ^ { \prime }$ . 

– entrypoints_ok $( \rho , \mathcal { G } ^ { \prime } )$ since entrypoints are unaffected. 

Case 9 WF -EFF -MERGE: 

We have 

A1 $E f f = f r e e z e ( ( ) x , u s e )$ 

A2 $\overline { { \Gamma } } = \Gamma : \overline { { \Gamma } } ^ { * }$ 

A3 Γ ⊢ use : ?? ⊣ Γ′ 

A4 cap(iso, ?? ) 

${ \mathrm { A } } 5 { \overline { { \Gamma } } } ^ { \prime } { \overline { { = } } } { \Gamma } ^ { \prime } { , } x : m a k e \_ i m m ( t ) : { \overline { { \Gamma } } } ^ { * }$ 

By REGION -MERGE 

A6 $r c f g = \langle ( r , S , F ) : : R S ; ( r , S ^ { \prime } ) * H _ { o p } ; R * H _ { c l } ; H _ { f r } \rangle$ 

A7 ?? fresh 

A8 $\mathbf { g e t } ( F , u s e ) = \left( ( k , \iota ) , F ^ { \prime } \right)$ 

A9 ?? ∈ dom(??.??) 

A10 $R ^ { \prime } = ( r , S ^ { \prime } \not  R . S )$ 

A11 ?? ′′ = ?? ′, ?? ↦→ (mut, ??) 

A12 $r c f g ^ { \prime } = \langle ( r , S , F ^ { \prime \prime } ) : : R S ; R * H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

Well-typedness holds by induction on the well-formedness of the top frame and region configuration heaps. 

A13 $\overline { { \Gamma } } ^ { \prime } ; \Delta ; \Psi r c f g ^ { \prime }$ 

We note 

A14 $\rho = \rho ( \overline { { \Gamma } } , r c f g ) = \rho ( \overline { { \Gamma } } ^ { \prime } , r c f g ^ { \prime } )$ 

A15 Cl = Closed (rcfg) 

${ \textrm A 1 6 ~ F r = F r o z e n ( r c f g ) = F r o z e n ( r c f g ^ { \prime } ) }$ 

A17 ?? ′ = ??.?? 

A18 $C l ^ { \prime } = C l - r$ 

A19 $\mathcal { G } = G ( r c f g )$ 

A20 $\mathcal { G } ^ { \prime } = G ( r c f g ^ { \prime } )$ 

A21 use = drop ?? 

By lemma F.14, 

$A 2 2 ~ L O C S = \{ l o c [ r ^ { \prime } ] ~ | ~ l o c [ r ^ { \prime } ] \in \mathcal { G } \}$ 

$A 2 3 ~ L O C S ^ { \prime } = \{ l o c [ r ] ~ | ~ l o c [ r ^ { \prime } ] \in L O C S \}$ 

${ \mathrm { A 2 4 ~ } } R E F S = \{ l o c { \xrightarrow { J , \kappa } } l o c ^ { \prime } | l o c \in L O C S \}$ 

$\mathbf { A } 2 5 \ R E F S ^ { \prime } = \left\{ l o c [ r ] \ \xrightarrow { f , k } \ l o c ^ { \prime } [ r _ { 2 } ^ { \prime } ] \ \left| \ l o c [ r ^ { \prime } ] \ \xrightarrow { f , k } \ l o c ^ { \prime } [ r _ { 2 } ] \in R E F S \ \wedge \ r _ { 2 } ^ { \prime } = \left[ \begin{array} { l l } { r } & { \mathrm { ~ i f ~ } r _ { 2 } = r ^ { \prime } } \\ { r _ { 2 } } & { \mathrm { ~ o t h e r w i s e ~ } } \end{array} \right] \right. \right.$ 

A26 $\displaystyle { \mathcal { G } } ^ { \prime } = { \mathcal { G } } - { \mathit { L O C S } } + { \mathit { L O C S } } ^ { \prime } - { \mathit { R } } { \dot { \boldsymbol { E } } } { \boldsymbol { F } } { \boldsymbol { S } } + { \mathit { R E F S } } ^ { \prime }$ 

$$
- \left(\operatorname{root} (r) \xrightarrow {z , \text {iso}} \operatorname{loc} _ {z} \left[ r ^ {\prime} \right]\right) + \left(\operatorname{root} (r) \xrightarrow {x , \text {mut}} \operatorname{loc} _ {z} [ r ]\right)
$$

Now, we can conclude that 

– var_unique $( { \mathcal { G } } ^ { \prime } )$ since each $r e f \in R E F S$ has been replaced with a corresponding one in $R E F S ^ { \prime } .$ . 

– region_order $( \rho , C l ^ { \prime } , F r , \mathcal { G } ^ { \prime } )$ by case analysis on the capability of the references in ????????′ reference $l o c _ { 1 } [ r _ { 1 } ] \stackrel { f , k } { \longrightarrow } l o c _ { 2 } [ r _ { 2 } ] \in R E F S ,$ , ?? = paused would violate region_order $( \rho , C l , F r , \mathcal { G } )$ since $r ^ { \prime } \in C l .$ . 

– location_ok $( \mathcal { G } ^ { \prime } )$ by same argument as var_unique. 

– deep_freeze $( F r , \mathcal { G } ^ { \prime } )$ since we have not changed frozen regions. 

– topology_ok $\mathrm { g r a p h } ( \rho , F r , \mathcal { G } ^ { \prime } )$ since intra-region references have been converted to intraregion references, and iso-references (i.e. inter-region references) have been converted to corresponding iso-references. 

– entrypoints $\mathbf { \sigma } _ { 0 } \mathbf { k } ( \rho , \vec { g } ^ { \prime } )$ since we have not touched entry points. 

Case 10 WF -EFF -HALLOC -ISO: 

${ \mathrm { A 1 ~ } } E f f = h a l l o c ( x , { \mathrm { i } } s 0 , \# C , u s e _ { 1 } , \ldots , u s e _ { n } )$ 

A2 $\overline { { \Gamma } } = \Gamma _ { 1 } : \overline { { \Gamma } } ^ { * }$ 

A3 ?? fresh 

A4 ⊢ iso ?? 

A5 $\mathbf { f t y p e s ( ( ) } C ) = f _ { 1 } : t _ { 1 } , \ldots , f _ { n } : t _ { n }$ 

A6 $\forall i \in [ 1 , n ] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } ^ { \prime } + \Gamma _ { i + 1 } \land \mathtt { c a p } ( i s o , i m m , t _ { i } ) \land t _ { i } ^ { \prime } < : t _ { i }$ 

A7 $\Gamma _ { n + 1 } , x : \mathfrak { i s o } C : \overline { { \Gamma } } ^ { * }$ 

From REGION -ALLOC -HEAP -ISO 

A8 $r c f g = \langle ( r , S , F _ { 1 } ) : \mathrel { \mathop : } R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

A9 ?? fresh 

A10 $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) = ( ( k _ { i } , \iota _ { i } ) , F _ { i + 1 } )$ 

A11 fields $( C ) = f _ { 1 } , . . . , f _ { n }$ 

A12 $o = ( \# C , [ f _ { 1 } \mapsto ( k _ { 1 } , \iota _ { 1 } ) , . . . , f _ { n } \mapsto ( k _ { n } , \iota _ { n } ) ] )$ 

A13 ?? fresh 

A14 ?? ′ fresh 

A15 $F ^ { \prime } = F _ { n + 1 } , x \mapsto ( \mathsf { i s o } , \iota )$ 

A16 $r c f g = \langle ( r , S , F _ { 1 } ) : : R S ; H _ { o p } ; ( r ^ { \prime } , [ \iota \mapsto o ] ) * H _ { c l } ; H _ { f r } \rangle$ 

Well-typedness follows similarly to case swap( ()), where we use an inductive argument to account for the multiple $u s e _ { 1 } , \ldots , u s e _ { n }$ . capability_ok and topology_ok follows from the observation that all references originating from the created object is either iso or imm, (i.e. 

$k _ { i } \in \{ \mathrm { i } \mathrm { s o } , \mathrm { i m m } \} )$ implying that they point into closed or frozen regions respectively. The object itself is created in a closed (fresh region), to which we create an iso reference. 


Case 11 WF -EFF -HALLOC -MU T:


A1 Eff = malloc(x, mut, #C, use $_{1}$ , $\ldots$ , use $_{n}$ )
A2 $\overline{\Gamma} = \Gamma_{1} :: \overline{\Gamma}^{*}$ A3 x fresh
A4 $\vdash$ mut C
A5 ftypes(()C) = f $_{1}$ : t $_{1}$ , $\ldots$ , f $_{n}$ : t $_{n}$ A6 $\forall i \in [1, n]$ . $\Gamma_{i} \vdash use_{i} : t_{i} \dashv \Gamma_{i+1}$ A7 $\Gamma_{n+1}$ , x : mut C :: $\overline{\Gamma}^{*}$ From REGION-ALLOC-HEAP-MUT 

A8 rcfg = $\langle(r, S, F_{1}) :: RS; (r, S') * H_{op}; H_{cl}; H_{fr} \rangle$ A9 x fresh 

A10 $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) = ( ( k _ { i } , \iota _ { i } ) , F _ { i + 1 } )$ 

A11 $\mathbf { f i e l d s } ( C ) = f _ { 1 } , . . . , f _ { n }$ 

A12 $o = ( \# C , [ f _ { 1 } \mapsto ( k _ { 1 } , \iota _ { 1 } ) , . . . , f _ { n } \mapsto ( k _ { n } , \iota _ { n } ) ] )$ 

A13 ?? fresh 

A14 $S ^ { \prime \prime } = S ^ { \prime } , \iota \mapsto o$ 

A15 $F ^ { \prime } = F _ { n + 1 } , x \mapsto ( \mathrm { m u t } , \iota )$ 

A16 $r c f g = \langle ( r , S , F _ { 1 } ) : : R S ; ( r , S ^ { \prime \prime } ) * H _ { o p } ; ( r ^ { \prime } , [ \iota \mapsto o ] ) * H _ { c l } ; H _ { f r } \rangle$ 

Follows from a similar argument as the mut case for swap(), using an inductive argument to account for the multiple use?? s. 


Case 12 WF -EFF -SALLOC:


A1 Eff = malloc(x, k, #C, use $_{1}$ , ..., use $_{n}$ )
A2 $\overline{\Gamma} = \Gamma_{1} :: \overline{\Gamma}^{*}$ A3 x fresh
A4 k ∈ {tmp, var}
A5 ⊢ k C 

A6 $\mathbf { f t y p e s ( ( ) } C ) = f _ { 1 } : t _ { 1 } , \ldots , f _ { n } : t _ { n }$ 

A7 $\forall i \in \left[ 1 , n \right] . \Gamma _ { i } \vdash u s e _ { i } : t _ { i } + \Gamma _ { i + 1 }$ 

A8 $\Gamma _ { n + 1 } , x : k C : : \overline { { \Gamma } } ^ { * }$ 

From REGION -ALLOC -TEMP 

A9 $r c f g = \langle ( r , S , F _ { 1 } ) : \mathrel { \mathop : } R S ; H _ { o p } ; H _ { c l } ; H _ { f r } \rangle$ 

A10 ?? fresh 

A11 ?? ∈ {tmp, var} 

A12 $\forall i \in [ 1 , n ] . \mathbf { g e t } ( F _ { i } , u s e _ { i } ) = ( ( k _ { i } , \iota _ { i } ) , F _ { i + 1 } )$ 

A13 $\mathbf { f i e l d s } ( C ) = f _ { 1 } , . . . , f _ { n }$ 

A14 $o = ( \# C , [ f _ { 1 } \mapsto ( k _ { 1 } , \iota _ { 1 } ) , . . . , f _ { n } \mapsto ( k _ { n } , \iota _ { n } ) ] )$ 

A15 ?? fresh 

A16 $S ^ { \prime } = S , \iota \mapsto o$ 

A17 $F ^ { \prime } = F _ { n + 1 } , x \mapsto ( k , \iota )$ 

A18 $r c f g = \langle ( r , S ^ { \prime } , F _ { 1 } ) : : R S ; H _ { o p } ; ( r ^ { \prime } , [ \iota \mapsto o ] ) * H _ { c l } ; H _ { f r } \rangle$ 

Follows from a similar argument as the tmp or var case for swap(), using an inductive argument to account for the multiple use?? s. 


Case 13 WF -EFF -LOAD:


A1 $\overline{\Gamma} = \Gamma :: \Gamma^{*}$ A2 x fresh 

A3 $\Gamma (y) = k$ CL  
A4 ftype( $CL,f) = t$ A5 $k\neq$ iso  
A6 $\vdash k\odot t$ By REGION-LOAD 

A7 rcfg = $\langle(r,S,F) :: RS;H_{op};H_{cl};H_{fr}\rangle$ A8 x fresh 

A9 $F(y) = (k_y,\iota_y)$ 

A10 cfg_load((r, S, F) :: RS; $H_{op} * H_{fr}, \iota_y$ ) = o[f $\mapsto (k_f, \iota_f)$ A11 $F' = F, x \mapsto (k_y \odot k_f, \iota_f)$ 

Well-typedness follows from a similar argument to the ???????? () cases. capability_ok and topology_ok follows from observation O14 . 

Case 14 WF -EFF -BIND: By trivial inductive argument on $\overline { { x = U s e } }$ 

Case 15 WF -EFF -BADEN TER: Trivial. 

Case 16 WF -EFF -EPS: Trivial. 

# F.4.3 Observations for Proof.

O1 

$$
\mathcal {G} \subseteq \mathcal {G} ^ {\prime} \land \text { var\_unique } (\mathcal {G} ^ {\prime}) \implies \text { var\_unique } (\mathcal {G})
$$

O2 If 

- $ref = loc_1 \xrightarrow{f,k} loc_2 \in \mathcal{G}$ - $k \neq \text{var}$ - $k' \neq \text{var}$ - $\text{var\_unique}(\mathcal{G})$ 

Then 

- var_unique( $\mathcal{G} + (_{ - } \xrightarrow{f',k'} loc_2)$ )
- var_unique( $\mathcal{G} - ref + (_{ - } \xrightarrow{f',k'} loc_2)$ ) 

Follows from $k \neq$ var implying that there is no reference $\_  { } \xrightarrow { g , k ^ { \prime \prime } } l o c _ { 2 }$ ??,?? ′′ with $k ^ { \prime \prime } = \mathsf { v a r }$ . 

O3 var_unique $( { \mathcal { G } } ) \ \wedge \ ( l o c _ { 1 } \ \xrightarrow { f , \mathrm { v a r } } \ l o c _ { 2 } ) \ \in { \mathcal { G } } \ \Longrightarrow \ \mathrm { v a r \_ u n i q u e } ( { \mathcal { G } } \ - \ ( l o c _ { 1 } \ \xrightarrow { f , \mathrm { v a r } } \ l o c _ { 2 } ) + ( l o c _ { 3 } \ \xrightarrow { f ^ { \prime } , k ^ { \prime } } ) ) ~ ,$ loc2) 

O4 If ref = (loc1 [??1] ?? ,??−−→ loc2 [??2]), ref ′ = (loc′ [?? ′] ?? ′,??′−−−→ loc2 [??2]), ref ∈ G, $\rho \vdash r _ { 2 } \leq r _ { 1 } \implies \rho \vdash r _ { 2 } \leq r _ { 1 } ^ { \prime }$ and 

$$
\text { topology\_ok\_graph } (\rho , F r, \mathcal {G})
$$

then 

$$
\text { topology\_ok\_graph } (\rho , F r, \mathcal {G} - r e f + r e f ^ {\prime})
$$

Follows from a simple case analysis on the clauses of topology_pw_ok. 

O5 $\mathrm { I f } \overline { { \Gamma } } _ { 1 } \overset { \mathsf { p w } } { \leq } \overline { { \Gamma } } _ { 2 }$ and ${ \overline { { \Gamma } } } _ { 1 } \vdash r c f g ,$ , then ${ \overline { { \Gamma } } } _ { 1 } | { \overline { { \Gamma } } } _ { 2 } \vdash r c f g$ . 

O6 If $\overline { { \Gamma } } _ { 1 } \overset { \mathtt { p w } } { \subseteq } \overline { { \Gamma } } _ { 2 } , \overline { { \Gamma } } _ { 1 } \vdash E f f \dashv \overline { { \Gamma } } _ { 1 } ^ { \prime }$ and $\overline { { { \Gamma } } } _ { 2 } \vdash E f f \ l + \overline { { { \Gamma } } } _ { 2 } ^ { \prime }$ , then $\overline { { \Gamma } } _ { 1 } ^ { \prime } \overset { \mathsf { p w } } { \subseteq } \overline { { \Gamma } } _ { 2 } ^ { \prime } .$ 

Follows by induction on $\overline { { \Gamma } } _ { 1 } \vdash E f f \dashv \overline { { \Gamma } } _ { 1 } ^ { \prime }$ . 

O7 If $\overline { { { \Gamma } } } [ x : t | t ^ { \prime } ] \vdash r c f \&$ then $\overline { { \Gamma } } [ \boldsymbol { x } : t ] \vdash r c f g$ or $\overline { { \Gamma } } [ x : t ^ { \prime } ] \vdash r c f g$ 

Follows since the actual type of ?? must be a subtype of $t | t ^ { \prime }$ , meaning that the actual type is a subtype of either ?? or $t ^ { \prime } .$ . 

O8 If $t < : t ^ { \prime }$ , then ?? ⊙ $t < : k \odot t ^ { \prime }$ 

O9 $t < : t ^ { \prime }$ and $\mathsf { c a p } ( i s o , t )$ implies that paused $\odot t = \downarrow$ . Follows by definition of subtyping and view-point adaptation (4). 

O10 $\Delta ; \Gamma \vdash F , \iota \notin$ dom(Δ) then $\Delta , \iota : C L ; \Gamma \vdash F .$ . Follows from that ?? ∉ dom(Δ) implies that ?? ∉ rng(?? ) 

O11 If $t < : t ^ { \prime }$ then make_mut ( ()?? ) <: make_mut ( ()?? ′). Follows by induction on structure of ?? . 

O12 If 

– entrypoints_ok(??, G) 

– topology_ok_graph(??, Fr, G) 

– ?? contains unique region ids 

– ?? = . . . :: ?? ′ . . . :: ?? ′′ :: . . . ?? ?? .?? ??.?? 

Then 

– ref G (root(?? ′), ?? ) = root(?? ′) ??,????−−−→ loc?? 

$- \ r e f _ { g } ( { \ r { \ r { r o o t } } } ( r ^ { \prime \prime } ) , y ) = { \ r { r o o t } } ( r ^ { \prime \prime } ) \xrightarrow { y , k _ { y } } l o c _ { y }$ ??,???? loc?? 

– Either loc?? ≠ loc?? or $f \neq g .$ 

Follows from uniqueness constraints of topology_ok_graph and that $\rho$ does not have repeated region ids. 

O13 If region_order1 $( r : : \rho , C l , F r , ( { \mathsf { r o o t } } ( r ) \xrightarrow { x , 1 s 0 } l o c _ { x } [ r ^ { \prime } ] ) )$ , then $r ^ { \prime } \in C l .$ 

Follows by definition of region_order1 

O14 If 

– ?? = ?? :: ?? ∗ 

– ⊢ ??, Cl, Fr 

– var_unique(G) 

– region_order(??, Cl, Fr, G) 

– location_ok(G) 

– deep_freeze(Fr, G) 

– topology_ok_graph $( \rho , F r , \mathcal { G } )$ 

– entrypoints_ok(??, G) 

– root(?? ) ??,????−−−→ loc?? [???? ] ∈ G 

– loc?? [???? ] ?? ,????−−−→ loc?? ∈ G 

– ???? ∈ ?? 

– ref = root(?? ) ??,???? ⊙ ????−−−−−−−→ loc?? 

– var_unique(G + ref ) 

– region_order(??, Cl, Fr, G + ref ) 

– location_ok(G + ref ) 

– deep_freeze(Fr, G + ref ) 

– topology_ok_graph(??, Fr, G + ref ) 

– entrypoints_ok $( \rho , \boldsymbol { \mathcal { G } } + r \boldsymbol { e } f )$ 

Follows from a case analysis on $k _ { x } , k _ { f }$ 

# F.5 Theorem: Soundness of the Tandem Semantics

A well formed configuration is either done, is has reached a failed state (because of failing to enter an already opened region), or can take a step to a well formed configuration. 

$$
\vdash \langle \{d e \} r c f g \rangle \implies d e = u s e \vee d e = \text { Failure } \vee \langle \{d e \} r c f g \rangle \rightarrow \langle \{d e ^ {\prime} \} r c f g ^ {\prime} \rangle \wedge \vdash \langle \{d e ^ {\prime} \} r c f g ^ {\prime} \rangle
$$

# F.5.1 Proof of Theorem. Follows from theorems F.1, F.2, F.3, and F.4.

# F.6 Lemma: Subtyping is Transitive

Subtyping is transitive, i.e. if $t _ { 1 } < : t _ { 2 }$ and $t _ { 2 } < : t _ { 3 }$ , then $t _ { 1 } < : t _ { 3 }$ . 

# F.6.1 Proof of Lemma. By strong induction over the subtyping relations.

# F.7 Lemma: get is Total

$$
\begin{array}{l} \Delta ; \Gamma \vdash F \land \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \implies \\ \exists F ^ {\prime}, k, \iota . \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime}) \\ \end{array}
$$

# F.7.1 Proof of Lemma. By induction over the well-formedness judgment of $F .$

# F.8 Lemma: Well-Formedness of get

$$
\begin{array}{l} \forall \Delta , \Gamma , \Gamma^ {\prime}, u s e, F, F ^ {\prime}, k, \iota . \\ \Delta ; \Gamma \vdash F \land \\ \Gamma \vdash u s e: t \dashv \Gamma^ {\prime} \wedge \\ \mathbf {g e t} (F, u s e) = ((k, \iota), F ^ {\prime}) \\ \Longrightarrow \\ \Delta ; \Gamma^ {\prime} \vdash F ^ {\prime} \wedge \\ \iota : C L \in \Delta \wedge \\ k C L <  : t \\ \end{array}
$$

# F.8.1 Observations. It is a simple exercise to prove the following observations

O1 If $\Delta ; \Gamma \ \vdash \ F ,$ , then if $\phi$ is a permutation of the order of the mappings in Γ and $F ,$ we have $\Delta ; \phi ( \Gamma ) \vdash \phi ( F )$ . 

O2 If $t < : t ^ { \prime } :$ , then $t [ k / k ^ { \prime } ] < : t ^ { \prime } [ k / k ^ { \prime } ]$ for all capabilities $k , k ^ { \prime } .$ 

# F.8.2 Proof of Lemma. In order to prove the lemma we begin by stating the assumptions

Main 1 $\Delta ; \Gamma \vdash F$ 

Main 2 Γ ⊢ use : $t \to \Gamma ^ { \prime }$ 

Main 3 $\mathbf { g e t } ( F , u s e ) = ( ( k , \iota ) , F ^ { \prime } ) .$ 

By structural induction on $\Delta ; \Gamma \vdash F :$ 

Case 1 WF -F -NIL:: Impossible. 

Case 2 WF -F -CONS:: 

$$
\begin{array}{l} \mathrm{A} 1 \Gamma = \Gamma_ {1}, y: t _ {y} \\ \mathrm{A2} F = F _ {1}, y \mapsto (k _ {y}, \iota_ {y}) \\ \mathrm{A} 3 \Delta ; \Gamma_ {1} \vdash F _ {1} \\ \mathrm{A} 4 \iota_ {y}: C L _ {y} \in \Delta \\ \mathrm{A5} k _ {y} C L _ {y} <  : t _ {y} \\ \mathrm{A6} y \notin \mathbf {d o m} (F _ {1}) \\ \end{array}
$$

We now split on cases of use. 

Case 2.1 use = ?? = ??: 

By lemma F.12 we we have 

B1 cap({mut, tmp, paused, imm}, $t _ { y } )$ 

B2 $\Gamma ^ { \prime } = \Gamma _ { 1 } , y : t _ { y }$ 

B3 $t _ { y } < : t$ 

By definition of get 

B4 ge $\cdot ( y , ( F _ { 1 } , y \mapsto ( k _ { y } , \iota _ { y } ) ) ) = ( ( k _ { y } , \iota _ { y } ) , ( F _ { 1 } , y \mapsto ( k _ { y } , \iota _ { y } ) ) )$ 

We can thus identify the following 

B5 $k = k _ { y }$ 

B6 ?? = ???? $\iota = \iota _ { y }$ 

B7 $C L = C L _ { y }$ 

B8 $F ^ { \prime } = F _ { 1 } , y \mapsto ( k _ { y } , \iota _ { y } )$ 

Together with these facts, and with observations O1, O2, and lemma F.6, it follows that 

$\ - \ \Delta ; \Gamma ^ { \prime } \vdash F ^ { \prime }$ 

$\ u \cdot \iota : C L \in \Delta ,$ and 

- ?? CL <: ?? . 

Case 2.2 use = ?? ≠ ??: 

Follows from definition of get and application of induction hypothesis together with observation O1. 

Case 2.3 use = drop ??, ?? = ??: 

This case is similar to Case 2.1 above . 

Case 2.4 use = drop ??, ?? ≠ ??: 

Similar to Case 2.2 . 

Case 3 WF -F -CONS -UNDEF: 

We have the assumptions 

A1 $\Gamma = \Gamma _ { 1 } , y$ : undef 

A2 $F = F _ { 1 } , y \mapsto$ undef 

A3 $\Delta ; \Gamma _ { 1 } \vdash F _ { 1 }$ 

A4 $\iota _ { y } : ~ C L _ { y } \in \Delta$ 

A5 $k _ { y } \ C L _ { y } < : t _ { y }$ 

A6 ?? ∉ dom(??1) 

Because ?? is undefined in both Γ and ?? , looking at the definition of get together with Main 2 , the only possible cases are use = drop ?? or use = ??, where $z \not = y$ . We can thus apply the induction hypothesis similar to Case 2.2 above. 

# F.9 Lemma: Well-Formed $\mathbf { c f g } .$ _load

$$
r c f g = \left\langle R S; H _ {o p}; H _ {c l}; H _ {f r} \right\rangle \wedge \overline {{\Gamma}}; \Delta ; \Psi \vdash r c f g \wedge \iota : C L \in \Delta \implies
$$

$$
\operatorname{cfg} _ {-} \operatorname{load} (R S, H _ {o p} * H _ {c l} * H _ {f r}) = (C L, F) \wedge \Delta ; \mathbf {f t y p e s} (C L) \vdash F
$$

F.9.1 Proof of Lemma. By induction over the well-formedness rules for RS and ?? . Note that every ?? in Δ maps to exactly one object in the configuration. 

# F.10 Lemma: Well-Formed Environment

If Γ ⊢ ?? : ?? ⊣ Γ′, then ⊢ Γ and $\vdash \Gamma ^ { \prime } .$ 

F.10.1 Proof of lemma. By structural induction on $\Gamma \vdash b : t \dashv \Gamma ^ { \prime }$ . 

# F.11 Lemma: Well-Formed Type Under Environment

If ⊢ Γ and $\Gamma ( x ) = t ,$ , then ⊢ ?? . 

F.11.1 Proof of lemma. By structural induction on $\vdash \Gamma .$ . 

# F.12 Inversion Lemma

Because of subtyping, we cannot invert typing judgments directly to get useful information. Instead we prove an inversion lemma that gives us the premises of the corresponding rule, with subtyping information regarding the result type. 

Because some of the type rules are only defined when variables have types of the shape ?? ????, we define a helper function for calculating the type of a field lookup for type unions (this is the result of typing *x.f when ?? has type ??): 

$$
\mathbf {f r e s u l t} (t, f) = \left\{ \begin{array}{l l} k \odot \mathbf {f t y p e} (C L, f) & \text { if } t = k C L \\ \mathbf {f r e s u l t} (t _ {1}, f) \mid \mathbf {f r e s u l t} (t _ {2}, f) & \text { if } t = t _ {1} \mid t _ {2} \end{array} \right.
$$

When doing strong updates of ref cells, we need to turn a union of types into a union of Cell types: 

$$
\begin{array}{l} \text { make\_cell } (k C L) = \text { var   Cell } [ k C L ] \\ \text { make\_cell } (t _ {1} \mid t _ {2}) = \text { make\_cell } (t _ {1}) \mid \text { make\_cell } (t _ {2}) \\ \end{array}
$$

When freezing a type, we need to change iso capabilities to imm: 

$$
\begin{array}{l} \text { make\_imm } (k C L) = \left\{ \begin{array}{l l} \text { imm } C L & \text { if } k = \text { iso } \\ k C L & \text { otherwise } \end{array} \right. \\ \text { make\_imm } (t _ {1} \mid t _ {2}) = \text { make\_imm } (t _ {1}) \mid \text { make\_imm } (t _ {2}) \\ \end{array}
$$

The inversion lemma itself is stated as follows: 

$$
\begin{array}{r c l} \Gamma \vdash x: t \dashv \Gamma^ {\prime} & \Longleftrightarrow & \vdash \Gamma \wedge \Gamma = \Gamma_ {1} [ x: t ^ {\prime} ] \wedge t ^ {\prime} <  : t \wedge \operatorname{cap} (\{\text {iso,var} \} ^ {c}, t ^ {\prime}) \wedge \\ & & \Gamma^ {\prime} = \Gamma_ {1} [ x: t ^ {\prime} ] \end{array}
$$

$$
\Gamma \vdash \mathbf {d r o p} x: t \dashv \Gamma^ {\prime} \quad \Longleftrightarrow \quad \vdash \Gamma \wedge \Gamma = \Gamma_ {1} [ x: t ^ {\prime} ] \wedge t ^ {\prime} <  : t \wedge \Gamma^ {\prime} = \Gamma_ {1} [ x: \mathbf {u n d e f} ]
$$

$$
\begin{array}{r c l} \Gamma \vdash * x. f: t \dashv \Gamma^ {\prime} & \Longrightarrow & \Gamma (x) = t _ {x} \wedge \mathbf {f r e s u l t} (t _ {x}, f) = t ^ {\prime} \wedge t ^ {\prime} <  : t \\ & & \operatorname{cap} (\{\text {iso} \} ^ {c}, t _ {x}) \wedge \forall k C L \in t _ {x}. \vdash k \odot t ^ {\prime} \end{array}
$$

$$
\begin{array}{r c l} \Gamma \vdash * x: t \dashv \Gamma^ {\prime} & \Longrightarrow & \Gamma (x) = t _ {x} \wedge \mathbf {f r e s u l t} (t _ {x}, \text { val }) = t ^ {\prime} \wedge t ^ {\prime} <  : t \\ & & \text { cap } (\text { var }, t _ {x}) \wedge \vdash \text { var } \odot t ^ {\prime} \end{array}
$$

$$
\begin{array}{r c l} \Gamma \vdash x. f := u s e: t \dashv \Gamma^ {\prime} & \Longrightarrow & \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime} \wedge \Gamma^ {\prime} (x) = t _ {x} \wedge \\ & & \operatorname{cap} (\{\operatorname{mut}, \operatorname{tmp} \}, t _ {x}) \wedge \mathbf {f r e s u l t} (t _ {x}, f) = t ^ {\prime} \wedge t ^ {\prime} <  : t \end{array}
$$

$$
\begin{array}{l l} \Gamma \vdash x := u s e: t \dashv \Gamma^ {\prime} & \Longrightarrow \quad \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime \prime} [ x: t _ {x} ] \wedge \operatorname{cap} (\operatorname{var}, t _ {x}) \wedge \\ & \mathbf {f r e s u l t} (t _ {x}, \operatorname{val}) = t _ {f} \wedge t _ {f} <  : t \wedge \\ & \Gamma^ {\prime} = \Gamma^ {\prime \prime} [ x: m a k e \_ c e l l [ t ^ {\prime} ] ] \end{array}
$$

$$
\begin{array}{l l} \Gamma_ {1} \vdash f n c (u s e _ {1},..., u s e _ {n}): t \dashv \Gamma_ {n + 1} & \implies \quad \mathbf {f n c t y p e} (f n c) = t _ {1},..., t _ {n} \to t ^ {\prime} \wedge t ^ {\prime} <  : t \wedge \\ & \forall i \in [ 1, n ]. \Gamma_ {i} \vdash u s e _ {i}: t _ {i} \dashv \Gamma_ {i + 1} \end{array}
$$

$$
\Gamma \vdash \mathbf {v a r} u s e: t \dashv \Gamma^ {\prime} \quad \Longrightarrow \quad \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime} \wedge \operatorname{varCell} [ t ^ {\prime} ] <  : t
$$

$$
\begin{array}{r c l} \Gamma_ {1} \vdash \mathbf {n e w}   k   C (\overline {{u s e}}): t \dashv \Gamma_ {n + 1} & \Longrightarrow & \vdash \Gamma_ {1} \wedge \vdash k   C   \wedge   k   C <  : t   \wedge   k \in \{\text { mut }, \text { tmp }, \text { iso } \} \wedge \\ & & \mathbf {f t y p e s} (C) = f _ {1}: t _ {1},... f _ {n}: t _ {n}   \wedge   \overline {{u s e}} = u s e _ {1},..., u s e _ {n} \wedge \\ & & \forall i \in [ 1, n ]. (\Gamma_ {i} \vdash u s e _ {i}: t _ {i} ^ {\prime} \dashv \Gamma_ {i + 1}   \wedge   t _ {i} ^ {\prime} <  : t _ {i}) \\ & & k = \text { iso } \Longrightarrow \text { cap } (\{\text { iso }, \text { imm } \}, t _ {i} ^ {\prime}) \end{array}
$$

$$
\Gamma \vdash \text { freeze   use }: t \dashv \Gamma^ {\prime} \quad \Longrightarrow \quad \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime} \wedge c a p (i s o, t ^ {\prime}) \wedge m a k e \_ i m m (t ^ {\prime}) <  : t)
$$

$$
\Gamma \vdash \text { merge } u s e: t \dashv \Gamma^ {\prime} \quad \Longrightarrow \quad \Gamma \vdash u s e: t ^ {\prime} \dashv \Gamma^ {\prime} \wedge \operatorname{cap} (i s o, t ^ {\prime}) \wedge m a k e \_ m u t (t ^ {\prime}) <  : t
$$

Continued on next page... 

$\Gamma_{1} \vdash \text{enter } x.f[\overline{y = use}]\{z => e\} : t \dashv \Gamma_{n+1} \implies$ $\forall i \in [1, n].\Gamma_{i} \vdash use_{i} : t_{i} \dashv \Gamma_{i+1} \land \Gamma_{n+1}(x) = t_{x} \land$ $\operatorname{cap}(\{\operatorname{mut}, \operatorname{tmp}, \operatorname{var}, \operatorname{paused}\}, t_{x}) \land$ $\mathbf{fresult}(t_x, f)) = t_f \land \operatorname{cap}(\operatorname{iso}, t_f) \land$ $\Gamma = y_1 : t_1', ..., y_n : t_n'$ where $t_i' = \begin{cases} t_i & \text{if cap}(\operatorname{iso}, t_i) \\ \text{paused} \odot t_i & \text{otherwise} \end{cases} \land$ $t_f' = make\_mut(t_f) \land \operatorname{cap}(\{\operatorname{iso}, \operatorname{imm}\}, t') \land t' <: t \land$ $\Gamma, z : \operatorname{tmp Cell}[t_f'] \vdash e : t' \dashv \Gamma', z : \operatorname{tmp Cell}[t_f']$ 

$\Gamma_{1} \vdash \text{enter } x [\overline{y = use}] \{z => e\} : t \dashv \Gamma' \implies$ $\forall i \in [1, n]. \Gamma_{i} \vdash use_{i}: t_{i} \dashv \Gamma_{i+1} \land \Gamma[x: t_{x}] = \Gamma_{n+1} \land$ $\operatorname{cap}(\operatorname{var}, t_{x}) \land$ $\mathbf{fresult}(t_{x}, \operatorname{val})) = t_{f} \land \operatorname{cap}(\operatorname{iso}, t_{f}) \land$ $\Gamma_{y} = y_{1}: t_{1}', ..., y_{n}: t_{n}'$ where $t_{i}' = \begin{cases} t_{i} & \text{if cap(iso, } t_{i}) \\ \text{paused} \odot t_{i} & \text{otherwise} \end{cases} \land$ $t_{z} = make\_cell(make\_mut(t_{f})) \land$ $t_{z}' = make\_cell(t_{f}') \land$ $\Gamma_{y}, z: t_{z} \vdash e: t' + \Gamma_{y}', z: t_{z}' \land \operatorname{cap}(\{\operatorname{iso}, \operatorname{imm}\}, t') \land$ $\operatorname{cap}(\operatorname{mut}, t_{f}') \land t' <: t \land$ $\Gamma' = \Gamma[x: t_{x}'] \land t' = make\_cell(make\_iso(t_{f}'))$ 

$\overline{\Gamma} \vdash \text{entered } x.f \ y.\text{val}\{de\} : t \vdash \Gamma' \implies$ $\overline{\Gamma} = \overline{\Gamma}' :: \Gamma_1 :: \Gamma_0 [x : t_x] \wedge$ $\text{cap}(\{\text{mut}, \text{tmp}, \text{var}, \text{paused}\}, t_x) \land \text{fresult}(t_x, f) = t_f \land$ $\text{cap}(\text{iso}, t) \land \overline{\Gamma}' :: \Gamma_1 \vdash de : t' + \Gamma_1' \land \text{cap}(\{\text{iso}, \text{imm}\}, t') \land$ $t' <: t \land \Gamma_1'(y) = t_y \land \text{fresult}(t_y, \text{val}) = t'' \land$ $\text{cap}(\text{mut}, t'') \land \text{cap}(\{\text{tmp}, \text{var}\}, t_y) \land \Gamma' = \Gamma_0 [x : t_x'] \land$ $t'_x = \begin{cases} \text{make\_cell}(\text{make\_iso}(t'')) & \text{if } t_x = \text{make\_cell}(\_) \\ t_x & \text{otherwise} \end{cases}$ 

F.12.1 Proof of Lemma. We first prove the ⇒-cases by induction over the typing judgments. The only two interesting subcases are using the rules CMD -T Y-SUB and CMD -T Y-SPLI T, which are similar for all the cases. We show the cases for ??, drop ?? and function call here; the remaining cases are similar. 

Case 1 $\Gamma \vdash x : t \vdash \Gamma ^ { \prime } :$ 

Case 1.a CMD -T Y-USE -KEEP: 

– Proof follows immediately from reflexivity of subtyping 

Case 1.b CMD -T Y-SUB: 

A1 $\Gamma \vdash x : t' \dashv \Gamma'$ A2 $t' <: t$ - By the induction hypothesis, we have $\Gamma(x) = t''$ and $t'' <: t'$ ,

- By transitivity of subtyping, we get $t'' <: t$ .

- The remaining obligations follow from the induction hypothesis. 

Case 1.c CMD -T Y-SPLI T: 

A1 $t = t _ { x } | t _ { x } ^ { \prime }$ 

A2 $\Gamma = \Gamma _ { 1 } \left[ y : t _ { y 1 } \mid t _ { y 2 } \right]$ 

A3 $\Gamma _ { 1 } \big [ y : t _ { y 1 } \big ] \dot { \textbf { \ i } } x : \dot { t } _ { x } \scriptsize \mathrm { ~ + ~ } \Gamma _ { 2 }$ 

A4 $\Gamma _ { 1 } \big [ y : t _ { y 2 } \big ] \vdash x : t _ { x } ^ { \prime } \vdash 1 \Gamma _ { 2 } ^ { \prime }$ 

A5 $\Gamma ^ { \prime } = \Gamma _ { 2 } \textrm { | } \Gamma _ { 2 } ^ { \prime }$ 

– if ?? ≠ ?? the proof follows from the induction hypothesis. 

– $\operatorname { I f } x = y ,$ by the induction hypothesis we have $t _ { y 1 } < : t _ { x }$ and $t _ { y 2 } < : t _ { x } ^ { \prime }$ , that iso is not in the capabilities of $t _ { y 1 }$ or $t _ { y 2 }$ , and that $\Gamma _ { 2 } ( x ) = t _ { y 1 }$ and $\Gamma _ { 2 } ^ { \prime } ( x ) = t _ { y 2 }$ . 

– From the definition of $\mathrm { ~ T _ { 2 } ~ } | ~ \Gamma _ { 2 } ^ { \prime } :$ , we get that $\Gamma ^ { \prime } ( x ) = t _ { y 1 } \mid { \dot { t } } _ { y 2 }$ 

– From the subtyping rules, we get that $t _ { y 1 } \mid t _ { y 2 } < : t _ { x } \mid t _ { x } ^ { \prime } .$ 

Case 2 Γ ⊢ drop $x : t \to \Gamma ^ { \prime } :$ 

Case 2.a CMD -T Y-DROP: 

– Proof follows immediately 

Case 2.b CMD -T Y-SUB: 

A1 Γ ⊢ drop $x : t ^ { \prime } \to \Gamma ^ { \prime }$ 

A2 $t ^ { \prime } < : t$ 

– By the induction hypothesis, we have $\Gamma ( x ) = t ^ { \prime \prime }$ and $t ^ { \prime \prime } < : t ^ { \prime } ,$ 

– By transitivity of subtyping, we get $t ^ { \prime \prime } < : t .$ 

– The remaining obligations follow from the induction hypothesis. 

Case 2.c CMD -T Y-SPLI T: 

A1 $t = t _ { x } | t _ { x } ^ { \prime }$ 

A2 $\Gamma = \Gamma _ { 1 } \left[ y : t _ { y 1 } \mid t _ { y 2 } \right]$ 

A3 $\Gamma _ { 1 } [ y : t _ { y 1 } ]$ ⊢ drop $x : t _ { x } \to \Gamma _ { 2 }$ 

A4 Γ1 [?? : ????2] ⊢ drop $x : t _ { x } ^ { \prime } \to \Gamma _ { 2 } ^ { \prime }$ 

A5 $\Gamma ^ { \prime } = \Gamma _ { 2 } \textrm { | } \Gamma _ { 2 } ^ { \prime }$ 

– if ?? ≠ ?? the proof follows from the induction hypothesis. 

– $\operatorname { I f } x = y ,$ by the induction hypothesis we have $t _ { y 1 } < : t _ { x }$ and $t _ { y 2 } < : t _ { x } ^ { \prime }$ , and that $\Gamma _ { 2 } ( x ) =$ undef and $\Gamma _ { 2 } ^ { \prime } ( x ) = \mathbf { u n d e f } .$ 

– From the definition of $\Gamma _ { 2 } \mid \Gamma _ { 2 } ^ { \prime } ;$ , we get that $\Gamma ^ { \prime } ( x ) = { \bf u n d e f } .$ 

– From the subtyping rules, we get that $t _ { y 1 } \mid t _ { y 2 } < : t _ { x } \mid t _ { x } ^ { \prime }$ 

$\mathbf { C a s e 3 } \ \Gamma \vdash f n c ( u s e _ { 1 } , . . . , u s e _ { n } ) : t \vdash \Gamma ^ { \prime } :$ 

Case 3.a CMD -T Y-CALL: 

– Proof follows immediately from reflexivity of subtyping 

Case 3.b CMD -T Y-SUB: 

A1 $\Gamma \vdash b : t ^ { \prime } \dashv \Gamma ^ { \prime }$ 

A2 $t ^ { \prime } < : t$ 

– By the induction hypothesis, we have fnctype $( f n c ) = t _ { 1 } , . . . , t _ { n }  t ^ { \prime \prime }$ and $t ^ { \prime \prime } < : t ^ { \prime } .$ 

– By transitivity of subtyping, we get $t ^ { \prime \prime } < : t .$ 

– The rest of the obligations follow from the induction hypothesis. 

Case 3.c CMD -T Y-SPLI T: 

A1 $t = t _ { b 1 } | t _ { b 1 } ^ { \prime }$ 

A2 $\Gamma = \Gamma _ { 1 } [ \stackrel {  } { x } : t _ { x 1 } \ | \ t _ { x 2 } ]$ 

A3 $\Gamma _ { 1 } \left[ \boldsymbol { x } : t _ { x 1 } \right] \vdash b : t _ { b 1 } \lnot \Gamma _ { 2 }$ 

A4 $\Gamma _ { 1 } \left[ \boldsymbol { x } : t _ { x 2 } \right] \vdash b : t _ { b 1 } ^ { \prime } \dashv { \Gamma } _ { 2 } ^ { \prime }$ 

A5 $\Gamma ^ { \prime } = \Gamma _ { 2 } \mid \Gamma _ { 2 } ^ { \prime }$ 

– By the induction hypothesis, we have $\mathbf { f n c t y p e } ( f n c ) = t _ { 1 } , . . . , t _ { n }  t ^ { \prime } .$ , and $t ^ { \prime } < : t _ { b 1 }$ and $t ^ { \prime } < : t _ { b 2 }$ . 

– By the subtyping rules, $t ^ { \prime } < : t _ { b 1 } | t _ { b 1 } ^ { \prime }$ . 

– The arguments are typed using the same reasoning as in the cases for $b = u s e ,$ but generalized to a sequence. We note that a drop of the same variable cannot appear twice in the same argument list. 

We then prove the ⇐-cases of ?? and drop ?? by induction on the shape of $t ^ { \prime } .$ 

Case 1 ??: 

Case 1.a $t ^ { \prime } = k C L { \mathrm { : } }$ : 

$\Gamma _ { 1 } [ x : k C L ] \vdash x : t + \Gamma _ { 1 } [ x : k C L ]$ by CMD -T Y-SUB with CMD -T Y-USE -KEEP. 

Case 1.b $t ^ { \prime } = t _ { 1 } ^ { \prime } \mid t _ { 2 } ^ { \prime } { \mathrm { : } }$ 

– By the induction hypothesis, we have $\Gamma _ { 1 } \left[ x \ : \ t _ { 1 } ^ { \prime } \right] \ \vdash \ x : \ t \ + \ \Gamma _ { 1 } \left[ x \ : \ t _ { 1 } ^ { \prime } \right]$ and $\Gamma _ { 1 } \left[ x : t _ { 2 } ^ { \prime } \right] \vdash x : t \dashv 1 \Gamma _ { 1 } \left[ x : t _ { 2 } ^ { \prime } \right] .$ . 

– By C M D -T Y- S P L I T we have Γ1 [?? : ?? ′1 | ?? ′2] ⊢ ?? : ?? | ?? ⊣ Γ1 [?? : ?? ′1 | ?? ′2]. 

– Since ?? | ?? <: ?? , by CMD -T Y-SUB we have $\Gamma _ { 1 } \left[ x : t _ { 1 } ^ { \prime } \mid t _ { 2 } ^ { \prime } \right] \vdash x : t + \Gamma _ { 1 } \left[ x : t _ { 1 } ^ { \prime } \mid t _ { 2 } ^ { \prime } \right]$ 

Case 2 drop ??: 

No induction is needed. We have $\Gamma [ x : t ^ { \prime } ] \vdash x : t \dashv \Gamma ^ { \prime } [ x$ : undef] by CMD -T Y-SUB with CMD -T Y-USE -DROP. 

# F.13 Graph Actions

To describe the action of an effect Eff on the object graph $\mathcal { G } _ { : }$ , we make some basic definitions of what an graph action is. The main idea is that each effect has some basic action consisting of removal and addition of locations and references. 

formed. By progress, WLOG we can assume that Given a configuration ${ \overline { { \Gamma } } } { \mathrm { ~ } } \vdash r c f g$ and $\overline { { { \Gamma } } } \vdash E f f \ l { \ l { 1 } } \overline { { { \Gamma } } } ^ { \prime }$ $r c f g \xrightarrow [ ] { E f f } r c f g ^ { \prime }$ , the graph . For such an $G ( r c f g )$ is well defined and well $E f f$ we can calculate a delta ?? such that $G ( r c f g ^ { \prime } ) = G ( r c f g ) + \delta$ (this addition is defined below). $\delta ,$ called a graph action, is a pair of set actions, one for locations and one for references. A set action is a pair of sets, one for removal and one for addition: 

?? ∈ (??LOCS, ??REFS) Graph action: action for locations/references 

???? ∈ P (??) × P (??) Set action: pair of remove and add set 

For convenience we define 

$$
\varepsilon = (\emptyset , \emptyset).
$$

Addition between a set ?? and a $\delta = \left( E _ { \mathrm { r } } , E _ { \mathrm { a } } \right)$ is defined as 

$$
E + (E _ {\mathrm{r}}, E _ {\mathrm{a}}) = (E \setminus E _ {\mathrm{r}}) \uplus E _ {\mathrm{a}}
$$

Given a graph action $\delta = ( \delta _ { l } , \delta _ { r } )$ and a graph $\mathcal { G } = ( \mathcal { L } , \mathcal { R } )$ , we define 

$$
\mathcal {G} + \delta = (\mathcal {L} + \delta_ {l}, \mathcal {R} + \delta_ {r})
$$

Furthermore for convenience, we define the addition of two actions $d e l t a _ { 1 } ~ = ~ ( E _ { 1 } ^ { \mathrm { r } } , E _ { 1 } ^ { \mathrm { a } } )$ and $\delta _ { 2 } = ( E _ { 2 } ^ { \mathrm r } , E _ { 2 } ^ { \mathrm a } )$ as 

$$
\delta_ {1} + \delta_ {2} = (E _ {1} ^ {\mathrm{r}} \uplus E _ {2} ^ {\mathrm{r}}, E _ {1} ^ {\mathrm{a}} \uplus E _ {2} ^ {\mathrm{a}})
$$

When not ambiguous, we can write 

$$
\begin{array}{l} \mathcal {G} + \mathcal {L} \equiv \mathcal {G} + ((\emptyset , \mathcal {L}), \varepsilon) \\ \mathcal {G} - \mathcal {L} \equiv \mathcal {G} + ((\mathcal {L}, \emptyset), \varepsilon) \\ \mathcal {G} + l o c \equiv \mathcal {G} + ((\emptyset , \{l o c \}), \varepsilon) \\ \mathcal {G} - l o c \equiv \mathcal {G} + ((\{l o c \}, \emptyset), \varepsilon) \\ \mathcal {G} + \mathcal {R} \equiv \mathcal {G} + (\varepsilon , (\emptyset , \mathcal {R})) \\ \mathcal {G} - \mathcal {R} \equiv \mathcal {G} + (\varepsilon , (\mathcal {R}, \emptyset)) \\ \mathcal {G} + r e f \equiv \mathcal {G} + (\varepsilon , (\emptyset , \{r e f \})) \\ \mathcal {G} - r e f \equiv \mathcal {G} + (\varepsilon , (\{r e f \}, \emptyset)) \\ \end{array}
$$

Both + and − associate to the left. 

# F.14 Lemma: Well-Defined Graph Actions

If 

$$
\begin{array}{l} - \overline {{\Gamma}} \vdash r c f g \\ - \overline {{\Gamma}} \vdash E f f + \overline {{\Gamma}} ^ {\prime} \\ - r c f g \xrightarrow {E f f} r c f g ^ {\prime} \\ \end{array}
$$

then there is a unique ??, such that 

$$
G (r c f g ^ {\prime}) = G (r c f g) + \delta
$$

Furthermore, this $\delta$ can be computed using only $G ( r c f g ) , \rho ( \overline { { \Gamma } } , r c f g ) , E f f$ , and access to any fresh ?? and ?? used in the step. 

F.14.1 Proof of Lemma. We prove this by defining a partial function, taking a $\rho = \rho ( { \overline { { \Gamma } } } , r c f g )$ , $\mathcal { G } = G ( r c f g )$ , and $E \mathcal { f }$ : 

$$
\delta^ {*} (\rho , \mathcal {G}, E f f) = (\delta_ {L O C}, \delta_ {R E F})
$$

In the cases below, we make statements of the form 

$$
\operatorname{ref} _ {\mathcal {G}} (l o c, f) = \left(l o c \xrightarrow {f , k _ {f}} l o c _ {f}\right)
$$

These equalities are well-defined because of the well formedness of $r c f g$ and $E \mathcal { f }$ . Furthermore, uniqueness of ?? follows from that $r e f _ { g }$ is a partial function. 

Case 1 enter $( w , k , y . f , \overline { { x = u s e } } )$ : 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, e n t e r (w, k, y. f, \overline {{z = u s e}})) = (\delta_ {L O C}, \delta_ {R E F})
$$

Looking at the rule REGION -EN TER -OK, the enter() effect adds two new locations: a new root $\left( \boldsymbol { r } ^ { \prime } \right)$ for the region $r ^ { \prime }$ on the region stack, and a temp $( r ^ { \prime } , \iota ^ { \prime \prime } )$ , representing the Cell holding the entry (with $\iota ^ { \prime \prime }$ a fresh object id). We do not remove any graph locations. I.e. 

$$
\delta_ {L O C} = (\{\}, \{\text { root } (r ^ {\prime}), \text { temp } (r ^ {\prime}, i ^ {\prime \prime}) \}).
$$

As for the references, looking at the rule again, we have the following: 

– For each ???? = drop $x _ { i } \in \overline { { z = u s e } }$ , we remove the reference 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), x _ {i}) = \left(\operatorname{root} (r) \xrightarrow {x _ {i} , k _ {i}} \operatorname{loc} _ {x _ {i}}\right)
$$

and add the reference 

$$
\operatorname{root} (r ^ {\prime}) \xrightarrow {z _ {i} , k} l o c _ {x _ {i}}
$$

We also note that well-formedness of the effect and configuration, re $\hat { \boldsymbol g } ^ { ( \mathrm { r o o t } ( r ) , x _ { i } ) }$ is well defined, and $k _ { i } = \mathrm { i } s _ { 0 }$ . 

For such an ??, we let 

$$
\delta_ {R E F} ^ {i} = \left\{ \begin{array}{l l} (\{\text { root } (r) \xrightarrow {x _ {i} , k _ {i}} l o c _ {x _ {i}} \}, \{\text { root } (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {x _ {i}} \}) & \text { if   } k _ {i} = \text { iso } \\ (\{\text { root } (r) \xrightarrow {x _ {i} , k _ {i}} l o c _ {x _ {i}} \}, \{\text { root } (r) \xrightarrow {z _ {i} , \text { paused } \odot k _ {i}} l o c _ {x _ {i}} \}) & \text { otherwise } \end{array} \right.
$$

– For each $z _ { i } = x _ { i } \in \overline { { z = u s e } } ,$ , if 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), x _ {i}) = \left(\operatorname{root} (r) \xrightarrow {x _ {i} , k _ {i}} \operatorname{loc} _ {x _ {i}}\right),
$$

we add the reference 

$$
\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {z _ {i} , \text { paused } \odot k _ {i}} \operatorname{loc} _ {x _ {i}}.
$$

Well-formedness implies that $r e f _ { g } ( \mathsf { r o o t } ( r ) , x _ { i } )$ and paused $\odot k _ { i }$ is well defined. For such an ??, we let 

$$
\delta_ {R E F} ^ {i} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {x _ {i}} \}).
$$

– We finally add a reference from root(?? ′) to temp $( r ^ { \prime } , \iota ^ { \prime \prime } )$ 

$$
\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {w , k} \operatorname{temp} \left(r ^ {\prime}, l ^ {\prime \prime}\right)
$$

(note that b.c. of well-formedness, $k \in \{ \mathsf { v a r } , \mathsf { t e m p } \} )$ and from temp $( r ^ { \prime } , \iota ^ { \prime \prime } )$ to $l o c _ { f } .$ 

$$
\operatorname{temp} \left(r ^ {\prime}, \iota^ {\prime \prime}\right) \xrightarrow {\text { val,mut }} \operatorname{loc} _ {f}
$$

where 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), y) = \left(\operatorname{root} (r) \xrightarrow {y , k _ {y}} \operatorname{loc} _ {y}\right)
$$

$$
\operatorname{ref} _ {\mathcal {G}} \left(\operatorname{loc} _ {y}, f\right) = \left(\operatorname{loc} _ {y} \xrightarrow {f , \text {iso}} \operatorname{loc} _ {f}\right).
$$

These are well defined because of well-formedness. 

We let 

$$
\delta_ {R E F} ^ {\text {entry}} = \left(\emptyset , \left\{\left(\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {w , k} \operatorname{temp} \left(r ^ {\prime}, \iota^ {\prime \prime}\right)\right), \left(\operatorname{temp} \left(r ^ {\prime}, \iota^ {\prime \prime}\right) \xrightarrow {\text {val,mut}} l o c _ {f}\right) \right\}\right)
$$

From the above we conclude that 

$$
\delta_ {R E F} = \sum_ {i = 1} ^ {l e n (\overline {{z = u s e}})} \delta_ {R E F} ^ {i} + \delta_ {R E F} ^ {\text {entry}}
$$

Case 2 badenter(): 

The rule REGION -EN TER -FAIL has $r c f g = r c f g ^ { \prime }$ and we conclude that 

$$
\delta^ {*} (\rho , \mathcal {G}, b a d e n t e r ()) = (\varepsilon , \varepsilon)
$$

i.e. $\delta _ { L O C } = \varepsilon$ and $\delta _ { R E F } = \varepsilon .$ 

Case 3 exit(??, use, $y . f , z . f ^ { \prime } )$ : 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, e x i t (x, u s e, y. f, z. f ^ {\prime})) = (\delta_ {L O C}, \delta_ {R E F})
$$

We must now consider two cases: REGION -EXI T-TEMP and REGION -EXI T-HEAP. By examining both rules, we see that the top region frame is popped. We can thus immediately conclude that root(?? ′) and temp-locations pertaining to the top region $r ^ { \prime }$ are removed, i.e. 

$$
\begin{array}{l} \delta_ {L O C} = \left(\left\{\operatorname{root} \left(r ^ {\prime}\right) \right\} \uplus \right. \\ \{\text { temp } (r ^ {\prime}, \_) \mid \text { temp } (r ^ {\prime}, \_) \in \mathcal {G} \}, \emptyset) \\ \end{array}
$$

For references, since we remove the top region frame all outgoing references are removed. This corresponds to: 

$$
\begin{array}{l} \delta_ {R E F} ^ {1} = \left(\left\{\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {- -} _ {-} \mid \operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {- -} _ {-} \in \mathcal {G} \right\} \uplus \right. \\ \left\{\operatorname{temp} \left(r ^ {\prime}, \_ \right.) \stackrel {{\rightarrow}} {{\longrightarrow}} \_ \mid \operatorname{temp} \left(r ^ {\prime}, \_ \right.) \stackrel {{\rightarrow}} {{\longrightarrow}} \_ \in \mathcal {G} \right\}, \emptyset) \\ \end{array}
$$

The return value specified by use is picked up from the frame being popped. Assuming use = ?? or use = drop ??, this means that we add a reference: 

$$
\delta_ {R E F} ^ {2} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {x , k _ {w}} l o c _ {w} \})
$$

where 

$$
\operatorname{ref} _ {\mathcal {G}} \left(\operatorname{root} \left(r ^ {\prime}\right), w\right) = \left(\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {w , k _ {w}} \operatorname{loc} _ {w}\right)
$$

Note that $r e f _ { \mathit { G } } ( \ r \circ \circ \ t ( r ^ { \prime } ) , z )$ is removed as part of $\delta _ { R E F } ^ { 1 }$ 

Lastly, we move the new entrypoint referred to by $z . f ^ { \prime }$ in the top frame, into ??.?? referred to in the second top frame: 

$$
\delta_ {R E F} ^ {3} = (\{l o c _ {y} \xrightarrow {f , k} l o c _ {f} \}, \{l o c _ {y} \xrightarrow {f , k} l o c _ {f ^ {\prime}} \})
$$

where 

$$
\begin{array}{l} \operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), y) = \left(\operatorname{root} (r) \xrightarrow {y , k _ {y}} \operatorname{loc} _ {y}\right) \\ \operatorname{ref} _ {\mathcal {G}} \left(\operatorname{loc} _ {y}, f\right) = \left(\operatorname{loc} _ {y} \xrightarrow {f , k} \operatorname{loc} _ {f}\right) \\ \operatorname{ref} _ {\mathcal {G}} \left(\operatorname{root} \left(r ^ {\prime}\right), z\right) = \left(\operatorname{root} \left(r ^ {\prime}\right) \xrightarrow {z , k _ {z}} \operatorname{loc} _ {f ^ {\prime}}\right) \\ \end{array}
$$

In total we get: 

$$
\delta_ {R E F} = \delta_ {R E F} ^ {1} + \delta_ {R E F} ^ {2} + \delta_ {R E F} ^ {3}
$$

Note that all of this holds for both REGION -EXI T-TEMP and REGION -EXI T-HEAP. The only difference between the two cases is the format of $l o c _ { y } .$ . 

Case 4 load(??, ??.?? ): 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, l o a d (x, y. f)) = (\delta_ {L O C}, \delta_ {R E F})
$$

It is clear from REGION -LOAD that 

$$
\delta_ {L O C} = \varepsilon .
$$

Furthermore, we see that the only action on references is adding a reference from root(?? ) to loc?? , the location referred to by $y . f { \mathrm { : } }$ : 

$$
\delta_ {R E F} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {x , k \odot k ^ {\prime}} l o c _ {f} \})
$$

where 

$$
\begin{array}{l} \operatorname{ref} (\operatorname{root} (r), y) = \left(\operatorname{root} (r) \xrightarrow {y , k} \operatorname{loc} _ {y}\right) \\ \operatorname{ref} \left(\operatorname{loc} _ {y}, f\right) = \left(\operatorname{loc} _ {y} \xrightarrow {f , k ^ {\prime}} \operatorname{loc} _ {f}\right) \\ \end{array}
$$

Case 5 swap(??, ??.?? , use): 

The two rules for swap are REGION -SWAP -TEMP and REGION -SWAP -HEAP, which are very similar. We note that swap() has no action on the locations of the graph: 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, s w a p (x, y. f, u s e)) = (\varepsilon , \delta_ {R E F})
$$

We have use = ?? or use = drop ??. If we let 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z) = \left(\operatorname{root} (r) \xrightarrow {z , k _ {z}} \operatorname{loc} _ {z}\right)
$$

we have two cases: 

$$
\delta_ {R E F} ^ {\text { drop }} = \left\{ \begin{array}{l l} (\{\text { root } (r) \xrightarrow {z , k _ {z}} l o c _ {z} \}, \emptyset) & \text { if   use } = \text { drop } z \\ \varepsilon & \text { otherwise } \end{array} \right.
$$

The action of the actual swap on the graph is 

$$
\delta_ {R E F} ^ {\text { switch }} = (\{l o c _ {y} \xrightarrow {f , k _ {y}} l o c _ {f} \}, \{l o c _ {y} \xrightarrow {f , k _ {z}} l o c _ {z} \})
$$

In total we have 

$$
\delta_ {R E F} = \delta_ {R E F} ^ {\mathrm{drop}} + \delta_ {R E F} ^ {\mathrm{switch}}.
$$

Because of the similarity, this holds in both REGION -SWAP -TMP and REGION -SWAP -HEAP. The only difference will be the format of $l o c _ { y }$ . 

Case 6 halloc(??, ??, #??, use): 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, h a l l o c (x, \# C, k, \overline {{u s e}})) = (\delta_ {L O C}, \delta_ {R E F}).
$$

By well-formedness, ?? = mut or ?? = iso, which corresponds to the two cases REGION - HALLOC -MU T and REGION -HALLOC -ISO respectively. In both cases the set of locations will be expanded with a new heap location with fresh object id ??. Its region id will depend on ??: 

$$
r ^ {*} = \left\{ \begin{array}{l l} r & \text { if   } k = \text { mut } \\ r ^ {\prime} & \text { if   } k = \text { iso,   where   } r ^ {\prime} \text {   is   fresh } \end{array} \right.
$$

$$
\delta_ {L O C} = (\emptyset , \{\text { heap } (r ^ {*}, \iota) \})
$$

Similar to the enter case ( Case 1 ) above, we have a sequence of use to consider. These correspond to the fields of the new object being created. Letting fnames $( C ) = f _ { 1 } , \ldots , f _ { n } \colon$ – For each drop $z _ { i } \in \overline { { u s e } } .$ , 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = \left(\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} \operatorname{loc} _ {i}\right)
$$

and 

$$
\delta_ {R E F} ^ {i} = \left(\left\{\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i} \right\}, \left\{\operatorname{heap} \left(r ^ {*}, \iota\right) \xrightarrow {f _ {i} , k _ {i}} l o c _ {i} \right\}\right)
$$

– For each $z _ { i } \in \overline { { u s e } } .$ 

$$
r e f _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = (\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i})
$$

and 

$$
\delta_ {R E F} ^ {i} = (\emptyset , \{\text { heap } (r ^ {*}, \iota) \xrightarrow {f _ {i} , k _ {i}} l o c _ {i} \})
$$

This is all well defined because of well-formedness. In total we have 

$$
\delta_ {R E F} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {x , k} \operatorname{heap} \left(r ^ {*}, \iota\right) \}) + \sum_ {i = 1} ^ {\text { len } (\overline {{u s e}})} \delta_ {R E F} ^ {i}
$$

Case 7 salloc(??, ??, #CL, use): 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, s a l l o c (x, k, C, \overline {{u s e}})) = (\delta_ {L O C}, \delta_ {R E F}).
$$

By similar reasoning to Case 6 , for a fresh ?? 

$$
\delta_ {L O C} = (\emptyset , \{\text { temp } (r, \iota) \})
$$

Letting fnames(??) = ??1, . . . , ????: 

– For each drop $z _ { i } \in \overline { { u s e } } ,$ 

$$
r e f _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = (\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i})
$$

and 

$$
\delta_ {R E F} ^ {i} = (\{\text {root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i} \}, \{\text {temp} (r, \iota) \xrightarrow {f _ {i} , k _ {i}} l o c _ {i} \})
$$

– For each $z _ { i } \in \overline { { u s e } } ,$ 

$$
r e f _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = (\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i})
$$

and 

$$
\delta_ {R E F} ^ {i} = (\emptyset , \{\text { temp } (r, \iota) \xrightarrow {f _ {i} , k _ {i}} l o c _ {i} \})
$$

In total: 

$$
\delta_ {R E F} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {x , k} \operatorname{temp} (r, \iota) \}) + \sum_ {i = 1} ^ {\text {len} (\overline {{u s e}})} \delta_ {R E F} ^ {i}
$$

We note that ?? ∈ {tmp, var} by well-formedness. 

Case 8 freeze(??, use): 

By inspecting REGION -FREEZE, 

$$
\delta^ {*} (r:: r n, \mathcal {G}, f r e e z e (x, u s e)) = (\varepsilon , \delta_ {R E F})
$$

i.e. ???????? = ??. 

By well-formedness, use = drop ?? and 

$$
r e f _ {\mathcal {G}} (\operatorname{root} (r), z) = (\operatorname{root} \xrightarrow {z , \text { iso }} l o c _ {z})
$$

From rule REGION -FREEZE we have that this reference is consumed and converted into a immutable reference: 

$$
\delta_ {R E F} = (\{\mathrm{root} (r) \xrightarrow {z , \mathrm{iso}} l o c _ {z} \}, \{\mathrm{root} (r) \xrightarrow {x , \mathrm{imm}} l o c _ {z} \})
$$

Case 9 merge(??, use): 

$$
\delta^ {*} (r:: r n, \mathcal {G}, m e r g e (x, u s e)) = (\delta_ {L O C}, \delta_ {R E F})
$$

By well-formedness, use = drop ??. By REGION -MERGE, 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z) = \left(\operatorname{root} (r) \xrightarrow {z , \text { iso }} \operatorname{heap} \left(r ^ {\prime}, \iota^ {\prime}\right)\right)
$$

The ??s for merging a region is somewhat tricky. In the configuration semantics, this ammounts to merging the subheap of the region ?? ′ into the the subheap of the currently active region ?? . This means that all locations ?????? [?? ′] will be converted to ?????? [?? ]. 

$$
D _ {L O C} = \{l o c [ r ^ {\prime} ] \mid l o c [ r ^ {\prime} ] \in \mathcal {G} \}
$$

We write 

$$
\delta_ {L O C} = (D _ {L O C}, D _ {L O C} [ r ^ {\prime} / r ])
$$

where $D _ { L O C } [ r ^ { \prime } / r ]$ is the set of elements of $D _ { L O C }$ but with ?? ′ substituted for ?? . 

For references we have the analogue 

$$
D _ {R E F} = \left\{\left(l o c _ {1} [ r _ {1} ] \xrightarrow {f , k} l o c _ {2} [ r _ {2} ]\right) \mid \left(l o c _ {1} [ r _ {1} ] \xrightarrow {f , k} l o c _ {2} [ r _ {2} ]\right) \in \mathcal {G} \wedge \left(r _ {1} = r ^ {\prime} \vee r _ {2} = r ^ {\prime}\right) \right\}
$$

We have 

$$
\delta_ {R E F} = (D _ {R E F}, D _ {R E F} [ r ^ {\prime} / r ])
$$

Case 10 cast(??, use, ?? ????): 

use can either be ?? or drop ??. From the rule REGION -CAST it is obvious that 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, c a s t (x, u s e, k C L)) = (\varepsilon , \delta_ {R E F})
$$

where 

$$
\delta_ {R E F} = \delta_ {R E F} ^ {\mathrm{drop}} + (\emptyset , \{\mathsf {r o o t} (r) \xrightarrow {x , k _ {z}} l o c _ {z} \}
$$

where 

$$
\begin{array}{l} \operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z) = \left(\operatorname{root} (r) \xrightarrow {z , k _ {z}} \operatorname{loc} _ {z}\right) \\ \delta_ {R E F} ^ {\text {drop}} \qquad = \left\{ \begin{array}{l l} (\{\text {root} (r) \xrightarrow {z , k _ {z}} l o c _ {z} \}, \emptyset) & \text {if use = \mathbf {d r o p} z} \\ \varepsilon & \text {otherwise} \end{array} \right. \\ \end{array}
$$

Case 11 nocast(??, use, ?? ????): 

By similar reasoning to Case 10 , using REGION -NOCAST instead: 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, n o c a s t (x, u s e, k C L)) = (\varepsilon , \delta_ {R E F})
$$

where 

$$
\delta_ {R E F} = \delta_ {R E F} ^ {\mathrm{drop}} + (\emptyset , \{\mathrm{root} (r) \xrightarrow {x , k _ {z}} l o c _ {z} \}
$$

where 

$$
\begin{array}{l} \operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z) = \left(\operatorname{root} (r) \xrightarrow {z , k _ {z}} \operatorname{loc} _ {z}\right) \\ \delta_ {R E F} ^ {\text {drop}} \qquad = \left\{ \begin{array}{l l} (\{\text {root} (r) \xrightarrow {z , k _ {z}} l o c _ {z} \}, \emptyset) & \text {if use = \mathbf {d r o p} z} \\ \varepsilon & \text {otherwise} \end{array} \right. \\ \end{array}
$$

Case 12 bind(?? = use): 

Looking at rule REGION -BIND, similarly to halloc() and salloc() cases ( Case 6 and Case 7 ), we have a sequence of uses. Here we instead create references from root(?? ). We get 

$$
\delta^ {*} (r:: \rho , \mathcal {G}, b i n d (\overline {{x = u s e}})) = (\varepsilon , \delta_ {R E F}).
$$

– For each ???? = drop ???? ∈ ?? = use, 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = \left(\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} \operatorname{loc} _ {i}\right)
$$

and 

$$
\delta_ {R E F} ^ {i} = \left(\{\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} l o c _ {i} \}, \{\operatorname{root} (r) \xrightarrow {x _ {i} , k _ {i}} l o c _ {i} \}\right)
$$

– For each ???? = ???? ∈ ?? = use, 

$$
\operatorname{ref} _ {\mathcal {G}} (\operatorname{root} (r), z _ {i}) = \left(\operatorname{root} (r) \xrightarrow {z _ {i} , k _ {i}} \operatorname{loc} _ {i}\right)
$$

and 

$$
\delta_ {R E F} ^ {i} = (\emptyset , \{\operatorname{root} (r) \xrightarrow {x _ {i} , k _ {i}} l o c _ {i} \})
$$

In total: 

$$
\delta_ {R E F} = \sum_ {i = 1} ^ {l e n (\overline {{u s e}})} \delta_ {R E F} ^ {i}
$$