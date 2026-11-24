package `in`.commandlinecoding.lipi.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import `in`.commandlinecoding.lipi.LipiApplication
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.ui.components.HomeBottomBar
import `in`.commandlinecoding.lipi.ui.enhance.EnhanceScreen
import `in`.commandlinecoding.lipi.ui.enhance.EnhanceViewModel
import `in`.commandlinecoding.lipi.ui.export.ExportScreen
import `in`.commandlinecoding.lipi.ui.export.ExportViewModel
import `in`.commandlinecoding.lipi.ui.adjust.AdjustScreen
import `in`.commandlinecoding.lipi.ui.camera.CameraScreen
import `in`.commandlinecoding.lipi.ui.components.HomeTab
import `in`.commandlinecoding.lipi.ui.library.LibraryScreen
import `in`.commandlinecoding.lipi.ui.library.LibraryViewModel
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens

object Routes {
    const val Library = "library"
    const val Settings = "settings"
    const val Camera = "camera"
    const val AdjustPattern = "adjust?page={page}&single={single}"
    const val Enhance = "enhance"
    const val Export = "export"

    /** Adjust corners from the first page that has no confirmed outline, or, with [single], for page [page] only (the crop tool). */
    fun adjust(page: Int = -1, single: Boolean = false) = "adjust?page=$page&single=$single"
}

/** Root of the app: the navigation graph. Screens that are not built yet show a placeholder. */
@Composable
fun LipiApp() {
    val nav = rememberNavController()
    val app = LocalContext.current.applicationContext as LipiApplication
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        NavHost(navController = nav, startDestination = Routes.Library) {
            composable(Routes.Library) {
                val vm: LibraryViewModel = viewModel(factory = viewModelFactory { initializer { LibraryViewModel(app.documents) } })
                val state by vm.state.collectAsState()
                HomeShell(HomeTab.Library, nav) { clearance ->
                    LibraryScreen(
                        state = state,
                        nowMillis = remember(state) { System.currentTimeMillis() },
                        onQueryChange = vm::onQueryChange,
                        onOpenSettings = { nav.navigate(Routes.Settings) { launchSingleTop = true } },
                        onOpenDocument = { },
                        onDeleteDocument = vm::delete,
                        bottomClearance = clearance,
                    )
                }
            }
            composable(Routes.Settings) { HomeShell(HomeTab.Settings, nav) { Placeholder(stringResource(R.string.settings_title)) } }
            composable(Routes.Camera) {
                CameraScreen(
                    session = app.session,
                    onClose = { nav.popBackStack() },
                    onReview = { nav.navigate(Routes.adjust()) { launchSingleTop = true } },
                )
            }
            composable(
                Routes.AdjustPattern,
                arguments = listOf(
                    navArgument("page") { type = NavType.IntType; defaultValue = -1 },
                    navArgument("single") { type = NavType.BoolType; defaultValue = false },
                ),
            ) { entry ->
                AdjustScreen(
                    session = app.session,
                    startPage = entry.arguments?.getInt("page") ?: -1,
                    single = entry.arguments?.getBoolean("single") ?: false,
                    onBack = { nav.popBackStack() },
                    onRetake = { nav.popBackStack() },
                    onDone = { nav.navigate(Routes.Enhance) { launchSingleTop = true } },
                )
            }
            composable(Routes.Enhance) {
                val vm: EnhanceViewModel = viewModel(factory = viewModelFactory { initializer { EnhanceViewModel(app.session) } })
                EnhanceScreen(
                    vm,
                    onBack = { nav.popBackStack() },
                    onDone = { nav.navigate(Routes.Export) { launchSingleTop = true } },
                    onCrop = { page -> nav.navigate(Routes.adjust(page = page, single = true)) { launchSingleTop = true } },
                    // Back to the camera, which still holds the pages so far; a new photo is added to them.
                    onAddPage = { if (!nav.popBackStack(Routes.Camera, false)) nav.navigate(Routes.Camera) },
                )
            }
            composable(Routes.Export) {
                val vm: ExportViewModel = viewModel(factory = viewModelFactory { initializer { ExportViewModel(app.session, app.documents, app.contentResolver) } })
                ExportScreen(
                    vm,
                    onBack = { nav.popBackStack() },
                    // Saved: the scan is in the library, so go there and drop the scanning screens from the back stack.
                    onSaved = { nav.navigate(Routes.Library) { popUpTo(Routes.Library) { inclusive = true }; launchSingleTop = true } },
                )
            }
        }
    }
}

/** A screen with the bottom bar and scan button. [content] receives the space the bar takes at the bottom. */
@Composable
private fun HomeShell(
    tab: HomeTab,
    nav: androidx.navigation.NavHostController,
    content: @Composable (bottomClearance: Dp) -> Unit,
) {
    val navBar = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()
    val clearance = LipiDimens.BottomBarHeight + navBar + 40.dp  // the scan button rises 28 dp above the bar
    Box(modifier = Modifier.fillMaxSize()) {
        content(clearance)
        HomeBottomBar(
            selected = tab,
            libraryLabel = stringResource(R.string.library_title),
            settingsLabel = stringResource(R.string.settings_title),
            scanDescription = stringResource(R.string.scan),
            onLibrary = { nav.navigate(Routes.Library) { popUpTo(Routes.Library) { inclusive = true }; launchSingleTop = true } },
            onSettings = { nav.navigate(Routes.Settings) { launchSingleTop = true } },
            onScan = { nav.navigate(Routes.Camera) { launchSingleTop = true } },
            modifier = Modifier.align(Alignment.BottomCenter),
        )
    }
}

@Composable
private fun Placeholder(title: String) {
    Box(modifier = Modifier.fillMaxSize().statusBarsPadding(), contentAlignment = Alignment.Center) {
        Text(title, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
