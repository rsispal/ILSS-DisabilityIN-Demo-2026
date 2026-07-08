# Forge 2.0 (`@forge/common`) — how to build with it

Honeywell's Forge component library. Components are **pre-styled** — you compose them and drive their
look through **props**, not utility classes. Import everything from the bundle global
`window.ForgeCommon.*` (e.g. `window.ForgeCommon.Button`).

## Setup & wrapping
- **No provider/theme wrapper is required.** Components render styled on their own — the brand tokens,
  fonts, and component CSS all arrive via `styles.css` (which `@import`s the Honeywell fonts and the
  compiled component + token CSS). Just render the component.
- **Brand font**: `'Honeywell Sans Web Common'` (Honeywell Sans, 9 weights) — already applied by the
  shipped CSS; don't override `font-family`.
- **Overlays portal to `document.body`**: `Modal`, `Tooltip`, `Popover`, `ContextualMenu`. `Modal` opens
  by mounting (no `open` prop) and renders a backdrop; `Menu` renders inline.

## The styling idiom — drive components with PROPS (not CSS classes)
This DS has **no utility-class API for you to use** — style each component through its documented props,
and use inline `style` or the design tokens only for your own layout glue (flex/grid/gap). Do **not**
assume Tailwind utilities exist in the rendered design. Key prop vocabularies:

- **Button** `variant`: `primary` | `secondary` | `tertiary` | `quiet-primary` | `quiet-secondary` |
  `destructive-primary` | `destructive-secondary` | `ai`; `size`: `regular` | `large`; plus `icon`,
  `iconPosition` (`left`|`right`|`top`), `selected`.
- **Badge** — text goes in **children**, color via `color`: `red` | `orange` | `blue` | `green` |
  `gray` | `yellow` (e.g. `<Badge color="green">Active</Badge>`); `outline` for the outline style.
- **Card** `variant`: `ai` | `quiet` | `ai-alt` | `ai-border` (or `quiet`); compose with `CardHeader`
  (`label`/`title`/`subtitle`/`avatar`/`slotRight`), `CardContent` (`x`/`y` spacing), `CardValue`.
- **Banner** `variant="inline"` + `type` (`warning`|`error`|`success`|`info`), with `title`/`description`.
- **Form fields**: wrap in `Control` → `Label` + the input + `ControlError`/`ControlHelper`/`ControlSuccess`.
  Inputs (`Input`, `Toggle`, `Checkbox`, `Select`, `NativeSelect`…) require an `id` and take native props
  (`placeholder`, `defaultValue`, `checked`). `Input` has `size` (`regular`|`large`), `error`, `success`,
  `prefix`/`suffix`.
- **Icons** (`CheckIcon`, `PlusIcon`, `BellIcon`, … 100+): size with `className` (`w-5 h-5`) or inline
  `style={{width,height}}`; color follows `currentColor`.

For custom layout, the design **tokens** are CSS custom properties on `:root` — `var(--color-blue-500)`,
`var(--color-button-base)`, `var(--color-semantic-good)`, `var(--color-typography-base)`,
`var(--color-application)` (full palette: blue/green/red/orange/purple/teal/pink/yellow/gray + cool/warm
variants, each 100–1100; plus `--color-button-*`, `--color-semantic-*`, `--color-typography-*`,
`--color-graphic-*`). Prefer component props first; reach for tokens only for surrounding chrome.

## Where the truth lives
- Per component: read its `<Name>.d.ts` (the exact props) and `<Name>.prompt.md` (usage) in this kit.
- Styling: `styles.css` and its `@import` closure (`_ds_bundle.css` = component + token CSS, `fonts/fonts.css`).

## Idiomatic snippet
```jsx
const { Card, CardHeader, CardContent, Badge, Button, Control, Label, Input } = window.ForgeCommon;

<Card style={{ maxWidth: 380 }}>
  <CardHeader title="Building A — Main Tower" subtitle="14 floors · Zone 3"
    slotRight={<Badge color="green">Online</Badge>} />
  <CardContent>
    <Control>
      <Label>Site name</Label>
      <Input id="site" defaultValue="Building A — Main Tower" />
    </Control>
    <div style={{ display: 'flex', gap: 12, marginTop: 16 }}>
      <Button variant="primary">Save changes</Button>
      <Button variant="quiet-secondary">Cancel</Button>
    </div>
  </CardContent>
</Card>
```

# ForgeCommon (@forge/common@0.1.217)

This design system is the published @forge/common React library, bundled as a single
browser global. All 238 components are the real upstream code.

## Where things are

- `_ds_bundle.js` — the whole-DS bundle at the project root; loads every component to `window.ForgeCommon`. First line is a `/* @ds-bundle: … */` metadata header.
- `styles.css` — the single stylesheet entry: it `@import`s the tokens, fonts, and component styles (`_ds_bundle.css`). Link this one file.
- `components/<group>/<Name>/<Name>.prompt.md` (example JSX + variants), `<Name>.d.ts` (types), `<Name>.html` (variant grid).
- `tokens/*.css` — CSS custom properties, names verbatim from upstream.
- `fonts/` — `@font-face` files + `fonts.css` (when the package ships fonts).

For a specific component, `read_file("components/<group>/<Name>/<Name>.prompt.md")`.

## Loading

Add these two lines to your page once (React must be on the page first):

```html
<link rel="stylesheet" href="styles.css">
<script src="_ds_bundle.js"></script>
```

Components are then available at `window.ForgeCommon.*`. Mount into a dedicated child node (e.g. `<div id="ds-root">`), not the host page's own React root, so the two trees don't collide:

```jsx
const { Accordion } = window.ForgeCommon;
ReactDOM.createRoot(document.getElementById('ds-root')).render(<Accordion />);
```

## Tokens

357 CSS custom properties from @forge/common. Names are
preserved verbatim from upstream. They are declared inside `_ds_bundle.css` (this DS ships one compiled stylesheet rather than separate token files).

- **color** (304): `--tw-border-spacing-x`, `--tw-border-spacing-y`, `--tw-ring-offset-color`, …
- **spacing** (3): `--tw-ring-inset`, `--tw-space-x-reverse`, `--tw-space-y-reverse`
- **shadow** (4): `--tw-ring-offset-shadow`, `--tw-ring-shadow`, `--tw-shadow`, …
- **other** (46): `--tw-translate-x`, `--tw-translate-y`, `--tw-rotate`, …

## Components

### general
- `Accordion`
- `AccordionItem`
- `AiIcon`
- `AlertCircleIcon`
- `AlertIcon`
- `AppIcon`
- `AreaChartIcon`
- `ArrowDownDiagonalIcon`
- `ArrowDownIcon`
- `ArrowLeftIcon`
- `ArrowRightIcon`
- `ArrowSortIcon`
- `ArrowUpDiagonalIcon`
- `ArrowUpIcon`
- `AttachIcon`
- `Avatar`
- `AvatarGroup`
- `AvatarIcon`
- `Badge`
- `Banner`
- `BarLoader`
- `BellIcon`
- `BellOffIcon`
- `BookmarkIcon`
- `Breadcrumbs`
- `Button`
- `Calendar`
- `CalendarIcon`
- `CameraIcon`
- `Card`
- `CardContent`
- `CardHeader`
- `CardValue`
- `Carousel`
- `CarouselSlide`
- `Cell`
- `ChatIcon`
- `Checkbox`
- `CheckIcon`
- `ChevronDownIcon`
- `ChevronLeftIcon`
- `ChevronRightIcon`
- `ChevronsDownIcon`
- `ChevronsLeftIcon`
- `ChevronsRightIcon`
- `ChevronsUpIcon`
- `ChevronUpIcon`
- `Chip`
- `ChipList`
- `ChipRemove`
- `CircularLoader`
- `ClockIcon`
- `CloseCircleIcon`
- `CloseIcon`
- `CollapsableCard`
- `CollapsableCardContent`
- `CollapsableSection`
- `ColumnResize`
- `ColumnsIcon`
- `ColumnSort`
- `Combobox`
- `ComboboxOption`
- `ComboboxOptionAction`
- `ComboboxOptionGroup`
- `CommentIcon`
- `ContentLoader`
- `ContextualMenu`
- `ContextualMenuContent`
- `ContextualMenuItem`
- `Control`
- `ControlError`
- `ControlHelper`
- `ControlSuccess`
- `CopyIcon`
- `CustomizeView`
- `CustomizeViewAsync`
- `CustomizeViewAsyncAction`
- `CustomizeViewAsyncItem`
- `CustomizeViewAsyncSearch`
- `CustomizeViewItem`
- `DatePicker`
- `DateRangePicker`
- `DensityLeastIcon`
- `DensityMediumIcon`
- `DensityMostIcon`
- `DescriptionIcon`
- `Divider`
- `DownloadIcon`
- `EditIcon`
- `EmptyState`
- `ErrorIcon`
- `FailedIcon`
- `FavoriteIcon`
- `FileUpload`
- `Filter`
- `FilterAscendingIcon`
- `FilterDescendingIcon`
- `FilterGroup`
- `FilterGroupLoadMore`
- `FilterGroupNoResults`
- `FilterGroupSearch`
- `FilterGroupSkeleton`
- `FilterIcon`
- `FilterRemoveIcon`
- `ForgeIcon`
- `GlobalNavigation`
- `GrabberIcon`
- `GraphIcon`
- `Grid`
- `GroupIcon`
- `Header`
- `HelpIcon`
- `HideIcon`
- `HistoryIcon`
- `HomeIcon`
- `HoneywellForgeAltIcon`
- `HoneywellForgeIcon`
- `ImportIcon`
- `IncompleteCircleIcon`
- `InfoIcon`
- `Input`
- `Label`
- `Layout`
- `LayoutProvider`
- `Link`
- `LinkIcon`
- `LockIcon`
- `Logo`
- `MapPinIcon`
- `Marker`
- `MaximizeIcon`
- `Menu`
- `MenuIcon`
- `MenuItem`
- `MenuSection`
- `MicrophoneIcon`
- `MinimizeIcon`
- `MinusIcon`
- `Modal`
- `ModalContent`
- `ModalFooter`
- `ModalHeader`
- `ModelCreationIcon`
- `MonitorIcon`
- `MoreHorizontalIcon`
- `MoreVerticalIcon`
- `NativeSelect`
- `Navigation`
- `NavigationItem`
- `NavigationSection`
- `NumberControl`
- `NumericBadge`
- `OpenInNewIcon`
- `PageLoader`
- `Pagination`
- `PauseIcon`
- `PinIcon`
- `PlayIcon`
- `PlusIcon`
- `Popover`
- `PopoverContainer`
- `PriorityBadge`
- `ProgressAltIcon`
- `ProgressIcon`
- `ProgressIndicator`
- `QuestionIcon`
- `RadioButton`
- `RadioButtonGroup`
- `RadioVisual`
- `RecentIcon`
- `RedoIcon`
- `RefreshIcon`
- `Scheduler`
- `Search`
- `SearchIcon`
- `SearchListAction`
- `SearchResult`
- `SearchResultGroup`
- `SearchResultList`
- `SegmentedControl`
- `SegmentedControlButton`
- `Select`
- `SelectOption`
- `SelectOptionAction`
- `SelectOptionGroup`
- `SelectOptionSearch`
- `SendIcon`
- `SettingsIcon`
- `ShareAltIcon`
- `ShareIcon`
- `ShowIcon`
- `Sidebar`
- `SignOutIcon`
- `SkeletonLoader`
- `SkipToContent`
- `SliderIcon`
- `SnackbarContainer`
- `SnackbarMessage`
- `SnackbarProvider`
- `SortAscendingIcon`
- `SortDescendingIcon`
- `SpeakerIcon`
- `StatusBadge`
- `Stepper`
- `StepperContent`
- `StopIcon`
- `SuccessIcon`
- `Tab`
- `Table`
- `TableBody`
- `TableCell`
- `TableContent`
- `TableHead`
- `TableHeaderCell`
- `TableRow`
- `Tabs`
- `TextArea`
- `TextOverflow`
- `ThumbsDownIcon`
- `ThumbsUpIcon`
- `TimePeriodIcon`
- `TimePicker`
- `ToastContainer`
- `ToastMessage`
- `ToastProvider`
- `Toggle`
- `Toolbar`
- `ToolJiraIcon`
- `Tooltip`
- `TrashIcon`
- `Tree`
- `TreeNode`
- `TrendIcon`
- `UndoIcon`
- `UnlinkIcon`
- `UnlockIcon`
- `UnpinIcon`
- `UploadIcon`
