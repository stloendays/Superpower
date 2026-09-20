import React from 'react';
import { useUserPreferences } from '@src/hooks';
import { Card, CardContent } from '@src/components/ui/card';
import { Typography } from '../ui';
import { AutomationService } from '@src/services/automation.service';
import { cn } from '@src/lib/utils';
import { createLogger } from '@extension/shared/lib/logger';

// Default delay values in seconds

const logger = createLogger('Settings');

const DEFAULT_DELAYS = {
  autoInsertDelay: 2,
  autoSubmitDelay: 2,
  autoExecuteDelay: 2
} as const;

const Settings: React.FC = () => {
  const { preferences, updatePreferences } = useUserPreferences();

  const handleAutomationToggle = (type: 'autoInsert' | 'autoSubmit' | 'autoExecute', enabled: boolean) => {
    const nextPreferences: Record<string, boolean> = { [type]: enabled };

    // Auto Submit only makes sense after a successful Auto Insert.
    if (type === 'autoSubmit' && enabled) {
      nextPreferences.autoInsert = true;
    }
    if (type === 'autoInsert' && !enabled) {
      nextPreferences.autoSubmit = false;
    }

    updatePreferences(nextPreferences);
    AutomationService.getInstance().updateAutomationStateOnWindow().catch(error => {
      logger.error('[Settings] Failed to sync automation toggle:', error);
    });
  };

  // Handle delay input changes
  const handleDelayChange = (type: 'autoInsert' | 'autoSubmit' | 'autoExecute', value: string) => {
    const delay = Math.min(60, Math.max(0, parseInt(value) || 0));
    logger.debug(`${type} delay changed to: ${delay}`);
    
    // Update user preferences store with the new delay
    updatePreferences({ [`${type}Delay`]: delay });

    // Store in localStorage
    try {
      const storedDelays = JSON.parse(localStorage.getItem('mcpDelaySettings') || '{}');
      localStorage.setItem('mcpDelaySettings', JSON.stringify({
        ...storedDelays,
        [`${type}Delay`]: delay
      }));
    } catch (error) {
      logger.error('[Settings] Error storing delay settings:', error);
    }

    // Update automation state on window
    AutomationService.getInstance().updateAutomationStateOnWindow().catch(console.error);
  };

  // Load stored delays on component mount, set default to 2 seconds if not set
  React.useEffect(() => {
    try {
      const storedDelays = JSON.parse(localStorage.getItem('mcpDelaySettings') || '{}');
      // If no stored delays, use defaults
      if (Object.keys(storedDelays).length === 0) {
        updatePreferences(DEFAULT_DELAYS);
        localStorage.setItem('mcpDelaySettings', JSON.stringify(DEFAULT_DELAYS));
      } else {
        // Use stored delays
        updatePreferences(storedDelays);
      }
    } catch (error) {
      logger.error('[Settings] Error loading stored delay settings:', error);
      // Set defaults on error
      updatePreferences(DEFAULT_DELAYS);
      localStorage.setItem('mcpDelaySettings', JSON.stringify(DEFAULT_DELAYS));
    }
  }, [updatePreferences]);

  return (
    <div className="p-4 space-y-4">
      <Card className="border-slate-200 dark:border-slate-700 dark:bg-slate-800">
        <CardContent className="p-4">
          <Typography variant="h4" className="mb-1 text-slate-700 dark:text-slate-300">
            Automation
          </Typography>
          <p className="mb-4 text-xs leading-5 text-slate-500 dark:text-slate-400">
            Automate the detected MCP workflow while keeping connection checks and duplicate-execution guards in place.
          </p>

          <div className="space-y-3">
            <label className="flex items-start justify-between gap-3 rounded-lg border border-slate-200 dark:border-slate-700 p-3 cursor-pointer">
              <div>
                <div className="text-sm font-medium text-slate-800 dark:text-slate-100">Auto Execute</div>
                <p className="mt-1 text-xs leading-5 text-slate-500 dark:text-slate-400">
                  Automatically run complete MCP function blocks after the connection preflight succeeds. Failed or ambiguous tool calls are never replayed automatically.
                </p>
              </div>
              <input
                type="checkbox"
                checked={preferences.autoExecute}
                onChange={event => handleAutomationToggle('autoExecute', event.target.checked)}
                className="mt-1 h-4 w-4 rounded border-slate-300 text-indigo-600 focus:ring-indigo-500"
              />
            </label>

            <label className="flex items-start justify-between gap-3 rounded-lg border border-slate-200 dark:border-slate-700 p-3 cursor-pointer">
              <div>
                <div className="text-sm font-medium text-slate-800 dark:text-slate-100">Auto Insert</div>
                <p className="mt-1 text-xs leading-5 text-slate-500 dark:text-slate-400">
                  Insert a successful MCP result back into the active AI composer automatically.
                </p>
              </div>
              <input
                type="checkbox"
                checked={preferences.autoInsert}
                onChange={event => handleAutomationToggle('autoInsert', event.target.checked)}
                className="mt-1 h-4 w-4 rounded border-slate-300 text-indigo-600 focus:ring-indigo-500"
              />
            </label>

            <label className="flex items-start justify-between gap-3 rounded-lg border border-slate-200 dark:border-slate-700 p-3 cursor-pointer">
              <div>
                <div className="text-sm font-medium text-slate-800 dark:text-slate-100">Auto Submit</div>
                <p className="mt-1 text-xs leading-5 text-slate-500 dark:text-slate-400">
                  Submit the AI composer only after Auto Insert succeeds. Enabling this also enables Auto Insert.
                </p>
              </div>
              <input
                type="checkbox"
                checked={preferences.autoSubmit}
                onChange={event => handleAutomationToggle('autoSubmit', event.target.checked)}
                className="mt-1 h-4 w-4 rounded border-slate-300 text-indigo-600 focus:ring-indigo-500"
              />
            </label>
          </div>
        </CardContent>
      </Card>

      <Card className="border-slate-200 dark:border-slate-700 dark:bg-slate-800">
        <CardContent className="p-4">
          <Typography variant="h4" className="mb-4 text-slate-700 dark:text-slate-300">
            Automation Delay Settings
          </Typography>
          
          <div className="space-y-4">
            {/* Auto Insert Delay */}
            <div>
              <label
                htmlFor="auto-insert-delay"
                className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1"
              >
                Auto Insert Delay (seconds)
              </label>
              <input
                id="auto-insert-delay"
                type="number"
                min="0"
                max="60"
                value={preferences.autoInsertDelay || 0}
                onChange={(e) => handleDelayChange('autoInsert', e.target.value)}
                disabled={false}
                className={cn(
                  "w-full p-2 text-sm border rounded-md",
                  "bg-white dark:bg-slate-900",
                  "border-slate-300 dark:border-slate-600",
                  "text-slate-900 dark:text-slate-100"
                )}
              />
              <p className="mt-1 text-xs text-slate-500 dark:text-slate-400">
                Delay before auto-inserting content
              </p>
            </div>

            {/* Auto Submit Delay */}
            <div>
              <label
                htmlFor="auto-submit-delay"
                className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1"
              >
                Auto Submit Delay (seconds)
              </label>
              <input
                id="auto-submit-delay"
                type="number"
                min="0"
                max="60"
                value={preferences.autoSubmitDelay || 0}
                onChange={(e) => handleDelayChange('autoSubmit', e.target.value)}
                disabled={false}
                className={cn(
                  "w-full p-2 text-sm border rounded-md",
                  "bg-white dark:bg-slate-900",
                  "border-slate-300 dark:border-slate-600",
                  "text-slate-900 dark:text-slate-100"
                )}
              />
              <p className="mt-1 text-xs text-slate-500 dark:text-slate-400">
                Delay before auto-submitting form
              </p>
            </div>

            {/* Auto Execute Delay */}
            <div>
              <label
                htmlFor="auto-execute-delay"
                className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1"
              >
                Auto Execute Delay (seconds)
              </label>
              <input
                id="auto-execute-delay"
                type="number"
                min="0"
                max="60"
                value={preferences.autoExecuteDelay || 0}
                onChange={(e) => handleDelayChange('autoExecute', e.target.value)}
                disabled={false}
                className={cn(
                  "w-full p-2 text-sm border rounded-md",
                  "bg-white dark:bg-slate-900",
                  "border-slate-300 dark:border-slate-600",
                  "text-slate-900 dark:text-slate-100"
                )}
              />
              <p className="mt-1 text-xs text-slate-500 dark:text-slate-400">
                Delay before auto-executing functions
              </p>
            </div>
          </div>
        </CardContent>
      </Card>
    </div>
  );
};

export default Settings;
