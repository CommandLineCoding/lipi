package `in`.commandlinecoding.lipi.ui.camera

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.Settings
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.view.LifecycleCameraController
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.LocalLifecycleOwner
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.LiveDetector
import `in`.commandlinecoding.lipi.scan.LiveOutline
import `in`.commandlinecoding.lipi.scan.PageDetector
import `in`.commandlinecoding.lipi.scan.ScanSession
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.library.rememberThumbnail
import `in`.commandlinecoding.lipi.ui.theme.CameraPalette
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.concurrent.Executors

private enum class Permission { Unknown, Granted, Denied }

/**
 * The camera: live preview, shutter, torch, import from the gallery and the count of pages captured so far.
 * Pages go into [session]; [onReview] opens them for adjusting.
 */
@Composable
fun CameraScreen(session: ScanSession, onClose: () -> Unit, onReview: () -> Unit) {
    val context = LocalContext.current
    val pages by session.pages.collectAsState()
    val scope = rememberCoroutineScope()
    var permission by remember {
        mutableStateOf(
            if (ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) Permission.Granted
            else Permission.Unknown,
        )
    }
    var confirmDiscard by remember { mutableStateOf(false) }
    var importFailed by remember { mutableStateOf(false) }

    val askPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        permission = if (granted) Permission.Granted else Permission.Denied
    }
    val pickPhotos = rememberLauncherForActivityResult(ActivityResultContracts.PickMultipleVisualMedia()) { uris ->
        scope.launch {
            val failures = uris.count { !session.addImported(context, it) }
            importFailed = failures > 0
        }
    }
    val openPicker = { pickPhotos.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)) }
    val close = { if (pages.isEmpty()) onClose() else confirmDiscard = true }

    LaunchedEffect(Unit) { if (permission == Permission.Unknown) askPermission.launch(Manifest.permission.CAMERA) }
    BackHandler(onBack = close)

    Box(modifier = Modifier.fillMaxSize().background(CameraPalette.Background)) {
        if (permission == Permission.Granted) {
            CameraPreview(session = session, pageCount = pages.size, lastPage = pages.lastOrNull()?.path, onClose = close, onReview = onReview, onPick = openPicker)
        } else {
            PermissionState(
                waiting = permission == Permission.Unknown,
                onAllow = { askPermission.launch(Manifest.permission.CAMERA) },
                onOpenSettings = {
                    context.startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", context.packageName, null)))
                },
                onPick = openPicker,
                onClose = close,
                pageCount = pages.size,
                onReview = onReview,
            )
        }
    }

    if (confirmDiscard) {
        AlertDialog(
            onDismissRequest = { confirmDiscard = false },
            title = { Text(stringResource(R.string.camera_discard_title)) },
            text = { Text(pluralStringResource(R.plurals.camera_discard_message, pages.size, pages.size)) },
            confirmButton = {
                TextButton(onClick = { confirmDiscard = false; session.clear(); onClose() }) { Text(stringResource(R.string.camera_discard)) }
            },
            dismissButton = { TextButton(onClick = { confirmDiscard = false }) { Text(stringResource(R.string.camera_keep_scanning)) } },
        )
    }
    if (importFailed) {
        AlertDialog(
            onDismissRequest = { importFailed = false },
            text = { Text(stringResource(R.string.camera_import_failed)) },
            confirmButton = { TextButton(onClick = { importFailed = false }) { Text(stringResource(R.string.ok)) } },
        )
    }
}

@Composable
private fun CameraPreview(session: ScanSession, pageCount: Int, lastPage: String?, onClose: () -> Unit, onReview: () -> Unit, onPick: () -> Unit) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val controller = remember {
        LifecycleCameraController(context).apply {
            setEnabledUseCases(LifecycleCameraController.IMAGE_CAPTURE or LifecycleCameraController.IMAGE_ANALYSIS)
            setImageAnalysisOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
            setImageAnalysisBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
        }
    }
    // The detector is loaded off the main thread; until it is there (or if this build has none) the camera works without an outline.
    val live by produceState<LiveDetector?>(initialValue = null) {
        value = withContext(Dispatchers.Default) { PageDetector.create(context)?.let { LiveDetector(it) } }
    }
    val outline by remember(live) { live?.outline ?: kotlinx.coroutines.flow.MutableStateFlow<LiveOutline?>(null) }.collectAsState()
    val analysisExecutor = remember { Executors.newSingleThreadExecutor() }
    DisposableEffect(live) {
        val detector = live
        if (detector != null) controller.setImageAnalysisAnalyzer(analysisExecutor, detector)
        onDispose {
            controller.clearImageAnalysisAnalyzer()
            detector?.close()
        }
    }
    DisposableEffect(Unit) { onDispose { analysisExecutor.shutdown() } }
    var torchOn by remember { mutableStateOf(false) }
    var capturing by remember { mutableStateOf(false) }
    var captureFailed by remember { mutableStateOf(false) }

    DisposableEffect(controller, lifecycleOwner) {
        controller.bindToLifecycle(lifecycleOwner)
        onDispose { controller.unbind() }
    }

    fun capture() {
        if (capturing) return
        capturing = true
        val file = session.newRawFile()
        controller.takePicture(
            ImageCapture.OutputFileOptions.Builder(file).build(),
            ContextCompat.getMainExecutor(context),
            object : ImageCapture.OnImageSavedCallback {
                override fun onImageSaved(output: ImageCapture.OutputFileResults) {
                    session.add(file)
                    capturing = false
                }

                override fun onError(exception: ImageCaptureException) {
                    file.delete()
                    capturing = false
                    captureFailed = true
                }
            },
        )
    }

    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { PreviewView(it).apply { this.controller = controller; scaleType = PreviewView.ScaleType.FIT_CENTER } },
        )
        OutlineOverlay(outline)

        Row(
            modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 16.dp, vertical = 12.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            CameraControl(R.drawable.ic_x, stringResource(R.string.close), onClose)
            CameraControl(
                icon = R.drawable.ic_zap,
                description = stringResource(if (torchOn) R.string.camera_torch_off else R.string.camera_torch_on),
                onClick = { torchOn = !torchOn; controller.enableTorch(torchOn) },
                tint = if (torchOn) CameraPalette.FlashOn else Color.White,
            )
        }

        Row(
            modifier = Modifier.align(Alignment.BottomCenter).fillMaxWidth().navigationBarsPadding().padding(horizontal = 32.dp, vertical = 28.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            GalleryButton(lastPage, stringResource(R.string.camera_import), onPick)
            Shutter(enabled = !capturing, description = stringResource(R.string.camera_capture), onClick = ::capture)
            PagesButton(pageCount, stringResource(R.string.camera_review), onReview)
        }
    }

    if (captureFailed) {
        AlertDialog(
            onDismissRequest = { captureFailed = false },
            text = { Text(stringResource(R.string.camera_capture_failed)) },
            confirmButton = { TextButton(onClick = { captureFailed = false }) { Text(stringResource(R.string.ok)) } },
        )
    }
}

@Composable
private fun PermissionState(
    waiting: Boolean,
    onAllow: () -> Unit,
    onOpenSettings: () -> Unit,
    onPick: () -> Unit,
    onClose: () -> Unit,
    pageCount: Int,
    onReview: () -> Unit,
) {
    Box(modifier = Modifier.fillMaxSize()) {
        Row(modifier = Modifier.statusBarsPadding().padding(16.dp)) { CameraControl(R.drawable.ic_x, stringResource(R.string.close), onClose) }
        if (!waiting) {
            Column(
                modifier = Modifier.align(Alignment.Center).padding(horizontal = 32.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(stringResource(R.string.camera_permission_title), style = MaterialTheme.typography.titleLarge, color = Color.White, textAlign = TextAlign.Center)
                Text(stringResource(R.string.camera_permission_body), style = MaterialTheme.typography.bodyMedium, color = CameraPalette.ModeInactive, textAlign = TextAlign.Center)
                Spacer(Modifier.height(8.dp))
                Button(
                    onClick = onAllow,
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = CameraPalette.Accent, contentColor = CameraPalette.OnAccent),
                ) { Text(stringResource(R.string.camera_permission_allow)) }
                OutlinedButton(onClick = onOpenSettings, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.camera_permission_settings), color = Color.White)
                }
                OutlinedButton(onClick = onPick, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.camera_permission_pick), color = Color.White)
                }
                if (pageCount > 0) {
                    TextButton(onClick = onReview) { Text(stringResource(R.string.camera_review_count, pageCount), color = Color.White) }
                }
            }
        }
    }
}

/** The 44 dp translucent round button on the camera. */
@Composable
private fun CameraControl(icon: Int, description: String, onClick: () -> Unit, tint: Color = Color.White) {
    Box(
        modifier = Modifier
            .size(44.dp)
            .background(CameraPalette.Control, CircleShape)
            .clickable(role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description },
        contentAlignment = Alignment.Center,
    ) { LipiIcon(icon, tint) }
}

/** The 76 dp white ring with a 58 dp disc inside. */
@Composable
private fun Shutter(enabled: Boolean, description: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .size(76.dp)
            .border(4.dp, Color.White, CircleShape)
            .clickable(enabled = enabled, role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description },
        contentAlignment = Alignment.Center,
    ) { Box(Modifier.size(58.dp).background(Color.White.copy(alpha = if (enabled) 1f else 0.5f), CircleShape)) }
}

/** Opens the photo picker; shows the latest page when there is one. */
@Composable
private fun GalleryButton(lastPage: String?, description: String, onClick: () -> Unit) {
    val thumb = rememberThumbnail(lastPage, 192)
    val shape = RoundedCornerShape(12.dp)
    Box(
        modifier = Modifier
            .size(48.dp)
            .clip(shape)
            .background(CameraPalette.Control)
            .border(2.dp, Color.White, shape)
            .clickable(role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description },
        contentAlignment = Alignment.Center,
    ) {
        if (thumb != null) Image(bitmap = thumb, contentDescription = null, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
        else LipiIcon(R.drawable.ic_folder, Color.White)
    }
}

/** The pages captured so far, with a count badge. Does nothing until there is a page. */
@Composable
private fun PagesButton(count: Int, description: String, onClick: () -> Unit) {
    Box(modifier = Modifier.size(48.dp)) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .background(CameraPalette.Control, CircleShape)
                .clickable(enabled = count > 0, role = Role.Button, onClick = onClick)
                .semantics { contentDescription = description },
            contentAlignment = Alignment.Center,
        ) { LipiIcon(R.drawable.ic_layers, Color.White.copy(alpha = if (count > 0) 1f else 0.5f)) }
        if (count > 0) {
            Box(
                modifier = Modifier.align(Alignment.TopEnd).size(20.dp).background(MaterialTheme.colorScheme.primary, CircleShape),
                contentAlignment = Alignment.Center,
            ) { Text(count.toString(), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onPrimary) }
        }
    }
}
