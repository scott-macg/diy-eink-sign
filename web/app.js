// PWA Application Controller
const API_BASE = '';

async function fetchStatus() {
  try {
    const res = await fetch(`${API_BASE}/api/status`);
    if (!res.ok) return;
    const data = await res.json();

    // Update Telemetry
    document.getElementById('statBattery').textContent = `${data.battery_pct}%`;
    document.getElementById('statAdc').textContent = `${data.battery_adc} mV`;
    document.getElementById('statReboots').textContent = data.reboot_count;
    document.getElementById('statEtag').textContent = data.etag ? data.etag.replace(/"/g, '') : '--';

    // Update Developer Mode UI
    const devToggle = document.getElementById('toggleDevMode');
    const badge = document.getElementById('modeBadge');
    devToggle.checked = data.developer_mode;

    if (data.developer_mode) {
      badge.textContent = 'DEV MODE';
      badge.className = 'badge badge-dev';
    } else {
      badge.textContent = 'PROD MODE';
      badge.className = 'badge badge-prod';
    }

    // Refresh Image Preview
    const preview = document.getElementById('displayPreview');
    preview.src = `${API_BASE}/api/render.png?t=${Date.now()}`;

    // Fill form if override active
    if (data.active_override) {
      document.getElementById('overrideMessage').value = data.active_override.message || '';
      document.getElementById('overrideSubtext').value = data.active_override.subtext || '';
    }
  } catch (err) {
    console.error('Failed to fetch status:', err);
  }
}

function refreshPreviewImage() {
  const preview = document.getElementById('displayPreview');
  if (preview) {
    preview.src = `${API_BASE}/api/render.png?t=${Date.now()}`;
  }
}

// Push Message Form
document.getElementById('overrideForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const message = document.getElementById('overrideMessage').value.trim();
  const subtext = document.getElementById('overrideSubtext').value.trim();
  const duration = parseInt(document.getElementById('overrideDuration').value, 10) || 60;

  if (!message) return;

  try {
    const res = await fetch(`${API_BASE}/api/override`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message, subtext, duration_minutes: duration })
    });
    if (res.ok) {
      refreshPreviewImage();
      await fetchStatus();
    }
  } catch (err) {
    console.error('Failed to push override:', err);
  }
});

// Push Random Quote Buttons
const handlePushQuote = async () => {
  try {
    const res = await fetch(`${API_BASE}/api/quotes/push_random`, { method: 'POST' });
    if (res.ok) {
      refreshPreviewImage();
      await fetchStatus();
    }
  } catch (err) {
    console.error('Failed to push quote:', err);
  }
};

const btnPushQuote = document.getElementById('btnPushQuote');
if (btnPushQuote) btnPushQuote.addEventListener('click', handlePushQuote);

const btnPushCurrentQuote = document.getElementById('btnPushCurrentQuote');
if (btnPushCurrentQuote) btnPushCurrentQuote.addEventListener('click', handlePushQuote);

// Image Upload Utility Modal Handlers
const modal = document.getElementById('imageUploadModal');
const btnOpenImageUpload = document.getElementById('btnOpenImageUpload');
const btnCloseModal = document.getElementById('btnCloseModal');

if (btnOpenImageUpload && modal) {
  btnOpenImageUpload.addEventListener('click', () => modal.classList.remove('hidden'));
}
if (btnCloseModal && modal) {
  btnCloseModal.addEventListener('click', () => modal.classList.add('hidden'));
}
if (modal) {
  modal.addEventListener('click', (e) => {
    if (e.target === modal) modal.classList.add('hidden');
  });
}

// Clear Override Button
document.getElementById('btnClearOverride').addEventListener('click', async () => {
  try {
    const res = await fetch(`${API_BASE}/api/override`, { method: 'DELETE' });
    if (res.ok) {
      document.getElementById('overrideMessage').value = '';
      document.getElementById('overrideSubtext').value = '';
      refreshPreviewImage();
      await fetchStatus();
    }
  } catch (err) {
    console.error('Failed to clear override:', err);
  }
});

// Developer Mode Toggle
document.getElementById('toggleDevMode').addEventListener('change', async (e) => {
  const isDev = e.target.checked;
  try {
    const res = await fetch(`${API_BASE}/api/devmode`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ developer_mode: isDev })
    });
    if (res.ok) {
      await fetchStatus();
    }
  } catch (err) {
    console.error('Failed to set dev mode:', err);
  }
});

// Register Service Worker
if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('/sw.js').catch(err => console.log('SW reg error:', err));
}

// Initial fetch and periodic polling every 10s
fetchStatus();
setInterval(fetchStatus, 10000);
